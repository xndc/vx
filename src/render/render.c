#include "render.h"
#include <glad/glad.h>
#include <stb/stb_sprintf.h>

#define X(name, path) Shader* name = NULL;
XM_ASSETS_SHADERS
#undef X

#define X(name, location, glsl_name) GLint name = location;
XM_ASSETS_SHADER_ATTRIBUTES
#undef X

#define X(name, glsl_name) GLint name = -1;
XM_ASSETS_SHADER_UNIFORMS
#undef X

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
    for (ptrdiff_t i = 0; i < arrlen(src); i++) {
        arrput(*dst, src[i]);
    }
}

static bool DefineArraysEqual (ShaderDefine* a, ShaderDefine* b) {
    if (arrlen(a) != arrlen(b)) {
        return false;
    }
    for (ptrdiff_t i = 0; i < arrlen(a); i++) {
        if (strcmp(a[i].name, b[i].name) != 0) { return false; }
        if (strcmp(a[i].text, b[i].text) != 0) { return false; }
    }
    return true;
}

static Shader* LoadShaderFromDisk (const char* name, const char* path) {
    Shader* shader = VXGENALLOC(1, Shader);
    shader->name = name;
    char* code = vxReadFile(path, true, NULL);
    shader->full = code;
    if (strncmp(code, "#version", 8) == 0) {
        char* newline = strchr(code, '\n');
        shader->body = newline + 1;
        size_t linesize = (size_t)(newline - code + 1);
        shader->version = VXGENALLOC(linesize + 1, char);
        strncpy(shader->version, code, linesize);
        shader->version[linesize] = '\0';
    } else {
        VXPANIC("Shader %s (%s) has no #version declaration", name, path);
    }
    VXINFO("Loaded shader %s from %s", name, path);
    return shader;
}

void InitRenderSystem() {
    #define X(name, path) name = LoadShaderFromDisk(#name, path);
    XM_ASSETS_SHADERS
    #undef X
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
    arrfree(S_RenderState.defines);
    S_RenderState = (RenderState){0};
    for (ptrdiff_t i = 0; i < arrlen(S_RenderStateStack); i++) {
        arrfree(S_RenderStateStack[i].defines);
    }
    arrfree(S_RenderStateStack);
}

void PushRenderState() {
    arrput(S_RenderStateStack, S_RenderState);
    arrlast(S_RenderStateStack).defines = NULL;
    DefineArrayCopy(S_RenderState.defines, &arrlast(S_RenderStateStack).defines);
}

void PopRenderState() {
    if (arrlen(S_RenderStateStack) > 0) {
        arrfree(S_RenderState.defines);
        S_RenderState = arrlast(S_RenderStateStack);
        arrpop(S_RenderStateStack);
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
        char* log = VXGENALLOC(logsize, char);
        glGetShaderInfoLog(sh, logsize, NULL, log);
        VXINFO("Compilation log for shader %s:\n%s", name, log);
        VXGENFREE(log);
    }
    if (ok) {
        VXINFO("Compiled vertex shader %s into OpenGL shader object %u", name, sh);
    } else {
        VXPANIC("Failed to compile vertex shader %s", name);
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
        char* log = VXGENALLOC(logsize, char);
        glGetProgramInfoLog(program, logsize, NULL, log);
        VXINFO("Link log for program (%s, %s):\n%s", vsh_name, fsh_name, log);
        VXGENFREE(log);
    }
    if (ok) {
        VXINFO("Linked program (%s, %s) into OpenGL object %u", vsh_name, fsh_name, program);
    } else {
        VXPANIC("Failed to link program (%s, %s)", vsh_name, fsh_name);
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
            ptrdiff_t pidx = hmgeti(S_ProgramCache, key);
            ProgramCacheValue* v = (pidx >= 0) ? &S_ProgramCache[pidx].value : NULL;
            if (v != NULL && DefineArraysEqual(v->defines, S_RenderState.defines)) {
                program = v->program;
            }
        }
        if (program == 0) {
            ShaderDefine* defines = S_RenderState.defines;
            // Generate #define block:
            static char define_block [16 * VX_KiB];
            size_t cursor_pos = 0;
            for (size_t i = 0; i < arrlenu(defines); i++) {
                char* cursor = &define_block[cursor_pos];
                size_t space = VXSIZE(define_block) - cursor_pos;
                size_t written = stbsp_snprintf(cursor, (int) space, "#define %s %s\n",
                    defines[i].name, defines[i].text);
                VXCHECK(written > 0);
                cursor_pos += written;
            }
            define_block[cursor_pos] = '\0';
            // Compile shaders and link program:
            const char* vs_src[] = {vsh->version, define_block, vsh->body};
            const char* fs_src[] = {fsh->version, define_block, fsh->body};
            GLuint glvs = CompileShader(vsh->name, GL_VERTEX_SHADER,   VXSIZE(vs_src), vs_src);
            GLuint glfs = CompileShader(fsh->name, GL_FRAGMENT_SHADER, VXSIZE(fs_src), fs_src);
            program = LinkProgram(vsh->name, glvs, fsh->name, glfs);
            // Save program to cache:
            ProgramCacheKey key = {vsh, fsh};
            ProgramCacheValue v = {NULL, program};
            DefineArrayCopy(defines, &v.defines);
            hmput(S_ProgramCache, key, v);
        }
    }
    // Retrieve uniforms and check attribute locations:
    ResetShaderVariables();
    if (program != 0) {
        #define X(name, glsl_name) name = glGetUniformLocation(program, glsl_name);
        XM_ASSETS_SHADER_UNIFORMS
        #undef X
        #define X(name, location, glsl_name) { \
            GLint loc = glGetAttribLocation(program, glsl_name); \
            if (!(loc == -1 || loc == location)) { \
                VXPANIC("Expected attribute %s of program %d to have location %d (is %d)", \
                    glsl_name, program, location, loc); \
            } \
        }
        XM_ASSETS_SHADER_ATTRIBUTES
        #undef X
    }
    S_RenderState.program = program;
    S_RenderState.current_vsh = vsh;
    S_RenderState.current_fsh = fsh;
    S_RenderState.defines_changed = false;
    glUseProgram(program);
}

void AddShaderDefine (const char* name, const char* text) {
    ShaderDefine def = {0};
    strcpy(def.name, name);
    strcpy(def.text, text);
    arrput(S_RenderState.defines, def);
    S_RenderState.defines_changed = true;
}

void ResetShaderDefines() {
    arrfree(S_RenderState.defines);
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
            VXPANIC("Invalid parameters (texture %u, sampler %u)", texture, sampler);
        }
    } else {
        VXWARN("Texture unit limit (%u) reached, not binding texture %u", max_units, texture);
    }
    return tu;
}

void SetUniformTexSampler (GLint uniform, GLuint texture, GLuint sampler) {
    if (texture != 0 && sampler != 0) {
        GLuint tu = BindTextureUnit(texture, sampler);
        glUniform1i(uniform, tu);
    }
}

void SetUniformTexture (GLint uniform, GLuint texture, GLuint min, GLuint mag, GLuint wrap) {
    if (texture != 0) {
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
    S_RenderState.next_texture_unit = 0;
    #define X(name, glsl_name) name = -1;
    XM_ASSETS_SHADER_UNIFORMS
    #undef X
    #define X(name, location, glsl_name) name = -1;
    XM_ASSETS_SHADER_ATTRIBUTES
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

    glUniform4fv(UNIF_CONST_DIFFUSE, 4, mat->const_diffuse);
    glUniform1f(UNIF_CONST_METALLIC,  mat->const_metallic);
    glUniform1f(UNIF_CONST_ROUGHNESS, mat->const_roughness);
    SetUniformTexSampler(UNIF_TEX_DIFFUSE,      mat->tex_diffuse,       mat->smp_diffuse);
    SetUniformTexSampler(UNIF_TEX_OCC_MET_RGH,  mat->tex_occ_met_rgh,   mat->smp_occ_met_rgh);
    SetUniformTexSampler(UNIF_TEX_OCCLUSION,    mat->tex_occlusion,     mat->smp_occlusion);
    SetUniformTexSampler(UNIF_TEX_METALLIC,     mat->tex_metallic,      mat->smp_metallic);
    SetUniformTexSampler(UNIF_TEX_ROUGHNESS,    mat->tex_roughness,     mat->smp_roughness);
    SetUniformTexSampler(UNIF_TEX_NORMAL,       mat->tex_normal,        mat->smp_normal);
}

void RenderMesh (Mesh* mesh) {
    if (mesh->gl_vertex_array != 0) {
        S_RenderState.next_texture_unit = 0;
        mat4 model;
        GetModelMatrix(model);
        VXCHECK(UNIF_MODEL_MATRIX >= 0);
        VXCHECK(UNIF_PROJ_MATRIX >= 0);
        VXCHECK(UNIF_VIEW_MATRIX >= 0);
        glUniformMatrix4fv(UNIF_MODEL_MATRIX, 1, false, model);
        glUniformMatrix4fv(UNIF_PROJ_MATRIX,  1, false, S_RenderState.mat_proj);
        glUniformMatrix4fv(UNIF_VIEW_MATRIX,  1, false, S_RenderState.mat_view);
        SetMaterial(mesh->material);
        glBindVertexArray(mesh->gl_vertex_array);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->indices.gl_object);
        switch (mesh->indices.type) {
            case FACCESSOR_UINT16:
            case FACCESSOR_UINT16_VEC3: {
                glDrawElements(GL_TRIANGLES, mesh->indices.count * mesh->indices.component_count,
                    GL_UNSIGNED_SHORT, NULL);
            } break;
            case FACCESSOR_UINT32:
            case FACCESSOR_UINT32_VEC3: {
                glDrawElements(GL_TRIANGLES, mesh->indices.count * mesh->indices.component_count,
                    GL_UNSIGNED_INT, NULL);
            } break;
            default: {
                VXWARN("Mesh 0x%lx has unknown index accessor type %d", mesh, mesh->indices.type);
            }
        }
    }
}