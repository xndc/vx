#include "render.h"
#include <glad/glad.h>
#include <stb_sprintf.h>

#define X(name, path) Shader* name = NULL;
XM_ASSETS_SHADERS
#undef X

#define X(name, location, glslname, gltfname) const GLint name = location;
XM_ATTRIBUTE_LOCATIONS
#undef X

#define X(name, glslname) GLint name = -1;
XM_ASSETS_SHADER_UNIFORMS
#undef X

// Should be set to either glTextureBarrier or glTextureBarrierNV by main().
PFNGLTEXTUREBARRIERPROC vxglTextureBarrier = NULL;

typedef struct {
    char name [64];
    char text [128];
} ShaderDefine;

typedef struct {
    ShaderDefine* defines;
    Shader* current_vsh;
    Shader* current_fsh;
    bool defines_changed;
    GLuint program;
    mat4 mat_view;
    mat4 mat_proj;
    mat4 mat_model;
    vec4 vec_model_rotation;
    vec3 vec_model_position;
    vec3 vec_model_scale;
    int next_texture_unit;
} RenderState;

typedef struct {
    Shader* vsh;
    Shader* fsh;
} ProgramCacheKey;

typedef struct {
    ShaderDefine* defines;
    GLuint program;
} ProgramCacheValue;

typedef struct {
    ProgramCacheKey key;
    ProgramCacheValue value;
} ProgramCacheEntry;

RenderState S_RenderState = {0};
RenderState* S_RenderStateStack = NULL;
ProgramCacheEntry* S_ProgramCache = NULL;

static void DefineArrayCopy (ShaderDefine* src, ShaderDefine** dst) {
    for (ptrdiff_t i = 0; i < stbds_arrlen(src); i++) {
        stbds_arrput(*dst, src[i]);
    }
}

static bool DefineArraysEqual (ShaderDefine* a, ShaderDefine* b) {
    if (stbds_arrlen(a) != stbds_arrlen(b)) {
        return false;
    }
    for (ptrdiff_t i = 0; i < stbds_arrlen(a); i++) {
        if (strcmp(a[i].name, b[i].name) != 0) { return false; }
        if (strcmp(a[i].text, b[i].text) != 0) { return false; }
    }
    return true;
}

Shader* LoadShaderFromDisk (const char* name, const char* path) {
    Shader* shader = calloc(1, sizeof(Shader));
    shader->name = name;
    char* code = vxReadFile(path, "r", NULL);
    shader->full = code;
    if (strncmp(code, "#version", 8) == 0) {
        char* newline = strchr(code, '\n');
        shader->body = newline + 1;
        size_t linesize = (size_t)(newline - code + 1);
        shader->version = calloc(linesize + 1, sizeof(char));
        strncpy(shader->version, code, linesize);
        shader->version[linesize] = '\0';
    } else {
        vxPanic("Shader %s (%s) has no #version declaration", name, path);
    }
    vxLog("Loaded shader %s from %s", name, path);
    return shader;
}

// Conceptually, starts a new render pass. Concretely, resets the following:
// -> the current render state and stack
// -> the currently selected shaders
// -> the current shader #define set
// -> the current model, view and projection matrices
// -> any current uniform and attribute locations (UNIF_*, ATTR_*)
// Note that this function does NOT reset the following:
// -> the currently bound framebuffer
// -> any other global OpenGL state (blend mode, depth test settings, etc.)
// TODO: signal to the debug system/profiler/RenderDoc/etc. that a pass is starting.
void StartRenderPass (const char* name) {
    ResetMatrices();
    ResetShaderDefines();
    SetRenderProgram(NULL, NULL);
    // Free render state and stack:
    stbds_arrfree(S_RenderState.defines);
    S_RenderState = (RenderState){0};
    for (ptrdiff_t i = 0; i < stbds_arrlen(S_RenderStateStack); i++) {
        stbds_arrfree(S_RenderStateStack[i].defines);
    }
    stbds_arrfree(S_RenderStateStack);
}

void PushRenderState() {
    stbds_arrput(S_RenderStateStack, S_RenderState);
    stbds_arrlast(S_RenderStateStack).defines = NULL;
    DefineArrayCopy(S_RenderState.defines, &stbds_arrlast(S_RenderStateStack).defines);
}

void PopRenderState() {
    if (stbds_arrlen(S_RenderStateStack) > 0) {
        stbds_arrfree(S_RenderState.defines);
        S_RenderState = stbds_arrlast(S_RenderStateStack);
        stbds_arrpop(S_RenderStateStack);
    }
}

static GLuint CompileShader (const char* name, GLenum type, size_t nsources, const char** sources) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, (GLsizei) nsources, sources, NULL);
    int ok, logsize;
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &logsize);
    if (logsize > 0) {
        char* log = calloc(logsize, sizeof(char));
        glGetShaderInfoLog(sh, logsize, NULL, log);
        vxLog("Compilation log for shader %s:\n%s", name, log);
        free(log);
    }
    if (ok) {
        vxLog("Compiled vertex shader %s into OpenGL shader object %u", name, sh);
    } else {
        vxPanic("Failed to compile vertex shader %s", name);
    }
    return sh;
}

static GLuint LinkProgram (const char* vsh_name, GLuint vsh, const char* fsh_name, GLuint fsh) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vsh);
    glAttachShader(program, fsh);
    int ok, logsize;
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logsize);
    if (logsize > 0) {
        char* log = calloc(logsize, sizeof(char));
        glGetProgramInfoLog(program, logsize, NULL, log);
        vxLog("Link log for program (%s, %s):\n%s", vsh_name, fsh_name, log);
        free(log);
    }
    if (ok) {
        vxLog("Linked program (%s, %s) into OpenGL object %u", vsh_name, fsh_name, program);
    } else {
        vxPanic("Failed to link program (%s, %s)", vsh_name, fsh_name);
    }
    return program;
}

void SetRenderProgram (Shader* vsh, Shader* fsh) {
    GLuint program = S_RenderState.program;
    if (vsh == NULL || fsh == NULL) {
        program = 0;
        vsh = NULL;
        fsh = NULL;
    } else {
        if (vsh != S_RenderState.current_vsh) { program = 0; }
        if (fsh != S_RenderState.current_fsh) { program = 0; }
        if (program == 0 || S_RenderState.defines_changed) {
            // Cache lookup:
            ProgramCacheKey key = {vsh, fsh};
            ptrdiff_t pidx = stbds_hmgeti(S_ProgramCache, key);
            ProgramCacheValue* v = (pidx >= 0) ? &S_ProgramCache[pidx].value : NULL;
            if (v != NULL && DefineArraysEqual(v->defines, S_RenderState.defines)) {
                program = v->program;
            }
        }
        if (program == 0) {
            // Generate #define block:
            ShaderDefine* defines = S_RenderState.defines;
            static char define_block [16 * VX_KiB];
            size_t cursor_pos = 0;
            for (size_t i = 0; i < stbds_arrlenu(defines); i++) {
                char* cursor = &define_block[cursor_pos];
                size_t space = vxSize(define_block) - cursor_pos;
                size_t written = stbsp_snprintf(cursor, (int) space, "#define %s %s\n",
                    defines[i].name, defines[i].text);
                vxCheck(written > 0);
                cursor_pos += written;
            }
            define_block[cursor_pos] = '\0';
            // Compile shaders and link program:
            const char* vs_src[] = {vsh->version, define_block, vsh->body};
            const char* fs_src[] = {fsh->version, define_block, fsh->body};
            GLuint glvs = CompileShader(vsh->name, GL_VERTEX_SHADER,   vxSize(vs_src), vs_src);
            GLuint glfs = CompileShader(fsh->name, GL_FRAGMENT_SHADER, vxSize(fs_src), fs_src);
            program = LinkProgram(vsh->name, glvs, fsh->name, glfs);
            // Save program to cache:
            ProgramCacheKey key = {vsh, fsh};
            ProgramCacheValue v = {NULL, program};
            DefineArrayCopy(defines, &v.defines);
            stbds_hmput(S_ProgramCache, key, v);
        }
    }
    // Retrieve uniforms and check attribute locations:
    ResetShaderVariables();
    if (program != 0) {
        #define X(name, glslname) name = glGetUniformLocation(program, glslname);
        XM_ASSETS_SHADER_UNIFORMS
        #undef X
        #define X(name, location, glslname, gltfname) { \
            GLint loc = glGetAttribLocation(program, glslname); \
            if (!(loc == -1 || loc == location)) { \
                vxPanic("Expected attribute %s of program %d to have location %d (is %d)", \
                    glslname, program, location, loc); \
            } \
        }
        XM_ATTRIBUTE_LOCATIONS
        #undef X
    }
    S_RenderState.program = program;
    S_RenderState.current_vsh = vsh;
    S_RenderState.current_fsh = fsh;
    S_RenderState.defines_changed = false;
    glUseProgram(program);
    // Set render target uniforms:
    #define X(name, format) \
        SetUniformTexture(UNIF_ ## name, name, GL_LINEAR, GL_LINEAR, GL_MIRRORED_REPEAT);
    XM_ASSETS_RENDERTARGETS_SCREENSIZE
    XM_ASSETS_RENDERTARGETS_SHADOWSIZE
    #undef X
}

void AddShaderDefine (const char* name, const char* text) {
    ShaderDefine def = {0};
    strcpy(def.name, name);
    strcpy(def.text, text);
    stbds_arrput(S_RenderState.defines, def);
    S_RenderState.defines_changed = true;
}

void ResetShaderDefines() {
    stbds_arrfree(S_RenderState.defines);
}

void SetViewMatrix (mat4 vmat)  { glm_mat4_copy(vmat, S_RenderState.mat_view); }
void SetProjMatrix (mat4 pmat)  { glm_mat4_copy(pmat, S_RenderState.mat_proj); }
void SetModelMatrix (mat4 mmat) { glm_mat4_copy(mmat, S_RenderState.mat_model); }
void GetModelMatrix (mat4 dest) { glm_mat4_copy(S_RenderState.mat_model, dest); }
void AddModelMatrix (mat4 mmat) { glm_mat4_mul(S_RenderState.mat_model, mmat, S_RenderState.mat_model); }
void AddModelPosition (vec3 position) { glm_translate(S_RenderState.mat_model, position); }
void AddModelRotation (vec4 rotation) { glm_quat_rotate(S_RenderState.mat_model, rotation, S_RenderState.mat_model); }
void AddModelScale (vec3 scale) { glm_scale(S_RenderState.mat_model, scale); }
void ResetModelMatrix() { glm_mat4_identity(S_RenderState.mat_model); }

void ResetMatrices() {
    glm_mat4_identity(S_RenderState.mat_view);
    glm_mat4_identity(S_RenderState.mat_proj);
    ResetModelMatrix();
}

void SetCameraMatrices (Camera* cam) {
    SetViewMatrix(cam->view_matrix);
    SetProjMatrix(cam->proj_matrix);
}

GLuint BindTextureUnit (GLuint texture, GLuint sampler) {
    GLuint tu = 0;
    int max_units;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_units);
    if (S_RenderState.next_texture_unit < max_units) {
        if (texture != 0 && sampler != 0) {
            tu = S_RenderState.next_texture_unit++;
            glActiveTexture(GL_TEXTURE0 + tu);
            glBindTexture(GL_TEXTURE_2D, texture);
            glBindSampler(tu, sampler);
        } else {
            vxPanic("Invalid parameters (texture %u, sampler %u)", texture, sampler);
        }
    } else {
        vxLog("Warning: Texture unit limit (%u) reached, not binding texture %u", max_units, texture);
    }
    return tu;
}

void SetUniformTexSampler (GLint uniform, GLuint texture, GLuint sampler) {
    if (uniform != -1) {
        if (texture != 0 && sampler != 0) {
            GLuint tu = BindTextureUnit(texture, sampler);
            glUniform1i(uniform, tu);
        } else {
            glUniform1i(uniform, 0);
        }
    }
}

void SetUniformTexture (GLint uniform, GLuint texture, GLuint min, GLuint mag, GLuint wrap) {
    if (uniform != -1 && texture != 0) {
        GLuint tu = S_RenderState.next_texture_unit;
        GLuint sampler = VXGL_SAMPLER[tu];
        glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, min);
        glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, mag);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, wrap);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, wrap);
        SetUniformTexSampler(uniform, texture, sampler);
    }
}

void ResetShaderVariables() {
    S_RenderState.next_texture_unit = 1;
    #define X(name, glsl_name) name = -1;
    XM_ASSETS_SHADER_UNIFORMS
    #undef X
}

void SetMaterial (Material* mat) {
    glDisable(GL_STENCIL_TEST);

    if (mat->blend) {
        glEnable(GL_BLEND);
        glBlendFunc(mat->blend_srcf, mat->blend_dstf);
    } else {
        glDisable(GL_BLEND);
    }

    if (mat->cull) {
        glEnable(GL_CULL_FACE);
        glCullFace(mat->cull_face);
    } else {
        glDisable(GL_CULL_FACE);
    }

    if (mat->depth_test) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(mat->depth_func);
        glDepthMask(mat->depth_write ? GL_TRUE : GL_FALSE);
    } else {
        glDisable(GL_DEPTH_TEST);
    }

    if (mat->stipple) {
        glUniform1i(UNIF_STIPPLE, 1);
        glUniform1f(UNIF_STIPPLE_HARD_CUTOFF, mat->stipple_hard_cutoff);
        glUniform1f(UNIF_STIPPLE_SOFT_CUTOFF, mat->stipple_soft_cutoff);
    } else {
        glUniform1i(UNIF_STIPPLE, 0);
    }

    glUniform4fv(UNIF_CONST_DIFFUSE, 1, (float*) mat->const_diffuse);
    glUniform1f(UNIF_CONST_METALLIC,  mat->const_metallic);
    glUniform1f(UNIF_CONST_ROUGHNESS, mat->const_roughness);
    SetUniformTexSampler(UNIF_TEX_DIFFUSE,      mat->tex_diffuse,       mat->smp_diffuse);
    SetUniformTexSampler(UNIF_TEX_OCC_MET_RGH,  mat->tex_occ_met_rgh,   mat->smp_occ_met_rgh);
    SetUniformTexSampler(UNIF_TEX_OCCLUSION,    mat->tex_occlusion,     mat->smp_occlusion);
    SetUniformTexSampler(UNIF_TEX_METALLIC,     mat->tex_metallic,      mat->smp_metallic);
    SetUniformTexSampler(UNIF_TEX_ROUGHNESS,    mat->tex_roughness,     mat->smp_roughness);
    SetUniformTexSampler(UNIF_TEX_NORMAL,       mat->tex_normal,        mat->smp_normal);
}

void RenderMesh (Mesh* mesh, int w, int h, float t, uint32_t frame) {
    if (mesh->gl_vertex_array && mesh->material) {
        S_RenderState.next_texture_unit = 1;
        mat4 model;
        GetModelMatrix(model);
        glUniformMatrix4fv(UNIF_MODEL_MATRIX, 1, false, (float*) model);
        glUniformMatrix4fv(UNIF_PROJ_MATRIX,  1, false, (float*) S_RenderState.mat_proj);
        glUniformMatrix4fv(UNIF_VIEW_MATRIX,  1, false, (float*) S_RenderState.mat_view);
        glUniform2f(UNIF_IRESOLUTION, (float) w, (float) h);
        glUniform1f(UNIF_ITIME, t);
        glUniform1ui(UNIF_IFRAME, frame);
        SetMaterial(mesh->material);
        glBindVertexArray(mesh->gl_vertex_array);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->gl_element_array);
        // TODO: Make this more generic (somehow...)
        switch (mesh->gl_element_type) {
            case FACCESSOR_UINT8:
            case FACCESSOR_UINT8_VEC3: {
                glDrawElements(mesh->type,
                    (GLsizei) mesh->gl_element_count * FAccessorComponentCount(mesh->gl_element_type),
                    GL_UNSIGNED_BYTE, NULL);
            } break;
            case FACCESSOR_UINT16:
            case FACCESSOR_UINT16_VEC3: {
                glDrawElements(mesh->type,
                    (GLsizei) mesh->gl_element_count * FAccessorComponentCount(mesh->gl_element_type),
                    GL_UNSIGNED_SHORT, NULL);
            } break;
            case FACCESSOR_UINT32:
            case FACCESSOR_UINT32_VEC3: {
                glDrawElements(mesh->type,
                    (GLsizei) mesh->gl_element_count * FAccessorComponentCount(mesh->gl_element_type),
                    GL_UNSIGNED_INT, NULL);
            } break;
            default: {
                vxLog("Warning: Mesh 0x%lx has unknown index accessor type %ju",
                    mesh, mesh->gl_element_type);
            }
        }
    }
}

void RenderModel (Model* model, int w, int h, float t, uint32_t frame) {
    for (size_t i = 0; i < model->meshCount; i++) {
        PushRenderState();
        AddModelMatrix(model->meshTransforms[i]);
        RenderMesh(&model->meshes[i], w, h, t, frame);
        PopRenderState();
    }
}

// Executes a full-screen pass using the current vertex and fragment shader.
// NOTE: The vertex shader should be set to VSH_FULLSCREEN_PASS before running this pass.
void RunFullscreenPass (int w, int h, float t, uint32_t frame) {
    static vao = 0;
    static ebo = 0;
    static Material mat = {0};
    static const float points[] = {
        -1.0f, -1.0f,
        -1.0f, +1.0f,
        +1.0f, -1.0f,
        +1.0f, +1.0f,
    };
    static const uint16_t triangles[] = {
        0, 2, 1,
        1, 2, 3,
    };
    if (!vao) {
        InitMaterial(&mat);
        mat.depth_test = false;
        GLuint positions;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &ebo);
        glGenBuffers(1, &positions);
        glBindBuffer(GL_ARRAY_BUFFER, positions);
        glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, false, 2 * sizeof(float), NULL);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(triangles), triangles, GL_STATIC_DRAW);
        #if 0
        FAccessorInit(&S_FullscreenPass_Mesh.positions, FACCESSOR_FLOAT32_VEC2, points, 0, 4, 0);
        FAccessorInit(&S_FullscreenPass_Mesh.indices, FACCESSOR_UINT16_VEC3, triangles, 0, 2, 0);
        UploadMeshToGPU(&S_FullscreenPass_Mesh);
        #endif
    }
    if (S_RenderState.current_vsh != VSH_FULLSCREEN_PASS) {
        vxLog("Warning: RunFullscreenPass requires the VSH_FULLSCREEN_PASS vertex shader to be used");
    }
    SetMaterial(&mat);
    glUniform2f(UNIF_IRESOLUTION, (float) w, (float) h);
    glUniform1f(UNIF_ITIME, t);
    glUniform1ui(UNIF_IFRAME, frame);
    glBindVertexArray(vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    vxglTextureBarrier(); // lets the shader both read & write to the same texture
    glDrawElements(GL_TRIANGLES, vxSize(triangles), GL_UNSIGNED_SHORT, NULL);
}
