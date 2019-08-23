#include "render.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

PFNGLTEXTUREBARRIERPROC vxglTextureBarrier = NULL;
int vxglMaxTextureUnits = 0;

Material MAT_FULLSCREEN_QUAD;
Mesh MESH_QUAD;
Mesh MESH_CUBE;

void InitRenderSystem() {
    // Retrieve OpenGL properties:
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &vxglMaxTextureUnits);
    
    // Retrieve the correct glTextureBarrier (regular or NV) function for this system:
    if (glfwExtensionSupported("GL_NV_texture_barrier")) {
        vxglTextureBarrier = (PFNGLTEXTUREBARRIERPROC) glTextureBarrierNV;
    } else if (glfwExtensionSupported("GL_ARB_texture_barrier")) {
        vxglTextureBarrier = glTextureBarrier;
    } else {
        vxPanic("This program requires the GL_*_texture_barrier extension.");
    }

    // Generate standard materials:
    InitMaterial(&MAT_FULLSCREEN_QUAD);
    MAT_FULLSCREEN_QUAD.depth_test = false;

    // Generate standard upwards-facing quad mesh:
    {
        static const float quadP[] = {
            -1.0f, 0.0f, -1.0f,
            -1.0f, 0.0f, +1.0f,
            +1.0f, 0.0f, -1.0f,
            +1.0f, 0.0f, +1.0f,
        };
        static const uint16_t quadT[] = {
            0, 2, 1,
            1, 2, 3,
        };
        MESH_QUAD.type = GL_TRIANGLES;
        MESH_QUAD.gl_element_count = vxSize(quadT);
        MESH_QUAD.gl_element_type = FACCESSOR_UINT16;
        glGenBuffers(1, &MESH_QUAD.gl_element_array);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, MESH_QUAD.gl_element_array);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadT), quadT, GL_STATIC_DRAW);
        glGenVertexArrays(1, &MESH_QUAD.gl_vertex_array);
        glBindVertexArray(MESH_QUAD.gl_vertex_array);
        GLuint quadPbuf = 0;
        glGenBuffers(1, &quadPbuf);
        glBindBuffer(GL_ARRAY_BUFFER, quadPbuf);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadP), quadP, GL_STATIC_DRAW);
        glEnableVertexAttribArray(ATTR_POSITION);
        glVertexAttribPointer(ATTR_POSITION, 3, GL_FLOAT, false, 3 * sizeof(float), NULL);
    }

    // Generate standard cube mesh:
    // https://en.wikibooks.org/wiki/OpenGL_Programming/Modern_OpenGL_Tutorial_06
    {
        static const float cubeP[] = {
            // front
            -1.0, -1.0,  1.0,
             1.0, -1.0,  1.0,
             1.0,  1.0,  1.0,
            -1.0,  1.0,  1.0,
            // top
            -1.0,  1.0,  1.0,
             1.0,  1.0,  1.0,
             1.0,  1.0, -1.0,
            -1.0,  1.0, -1.0,
            // back
             1.0, -1.0, -1.0,
            -1.0, -1.0, -1.0,
            -1.0,  1.0, -1.0,
             1.0,  1.0, -1.0,
            // bottom
            -1.0, -1.0, -1.0,
             1.0, -1.0, -1.0,
             1.0, -1.0,  1.0,
            -1.0, -1.0,  1.0,
            // left
            -1.0, -1.0, -1.0,
            -1.0, -1.0,  1.0,
            -1.0,  1.0,  1.0,
            -1.0,  1.0, -1.0,
            // right
             1.0, -1.0,  1.0,
             1.0, -1.0, -1.0,
             1.0,  1.0, -1.0,
             1.0,  1.0,  1.0,
        };
        float cubeTC0[2*4*6] = {
            0.0, 0.0,
            1.0, 0.0,
            1.0, 1.0,
            0.0, 1.0,
        };
        for (int i = 1; i < 6; i++) {
            memcpy(&cubeTC0[i*4*2], &cubeTC0[0], 2*4*sizeof(GLfloat));
        }
        static const uint16_t cubeT[] = {
            // front
            0,  1,  2,
            2,  3,  0,
            // top
            4,  5,  6,
            6,  7,  4,
            // back
            8,  9,  10,
            10, 11, 8,
            // bottom
            12, 13, 14,
            14, 15, 12,
            // left
            16, 17, 18,
            18, 19, 16,
            // right
            20, 21, 22,
            22, 23, 20,
        };
        MESH_CUBE.type = GL_TRIANGLES;
        MESH_CUBE.gl_element_count = vxSize(cubeT);
        MESH_CUBE.gl_element_type = FACCESSOR_UINT16;
        glGenBuffers(1, &MESH_CUBE.gl_element_array);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, MESH_CUBE.gl_element_array);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeT), cubeT, GL_STATIC_DRAW);
        glGenVertexArrays(1, &MESH_CUBE.gl_vertex_array);
        glBindVertexArray(MESH_CUBE.gl_vertex_array);
        GLuint cubePbuf = 0;
        glGenBuffers(1, &cubePbuf);
        glBindBuffer(GL_ARRAY_BUFFER, cubePbuf);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cubeP), cubeP, GL_STATIC_DRAW);
        glEnableVertexAttribArray(ATTR_POSITION);
        glVertexAttribPointer(ATTR_POSITION, 3, GL_FLOAT, false, 3 * sizeof(float), NULL);
        GLuint cubeTC0buf = 0;
        glGenBuffers(1, &cubeTC0buf);
        glBindBuffer(GL_ARRAY_BUFFER, cubeTC0buf);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cubeTC0), cubeTC0, GL_STATIC_DRAW);
        glEnableVertexAttribArray(ATTR_TEXCOORD0);
        glVertexAttribPointer(ATTR_TEXCOORD0, 2, GL_FLOAT, false, 2 * sizeof(float), NULL);
    }
}

void StartRenderPass (RenderState* rs, const char* passName) {
    rs->program = 0;
    rs->material = NULL;
    glm_mat4_identity(rs->matView);
    glm_mat4_identity(rs->matViewInv);
    glm_mat4_identity(rs->matViewLast);
    glm_mat4_identity(rs->matProj);
    glm_mat4_identity(rs->matProjInv);
    glm_mat4_identity(rs->matProjLast);
    glm_mat4_identity(rs->matModel);
    glm_mat4_identity(rs->matModelLast);
    rs->nextFreeTextureUnit = 0;
}

void CopyRenderState (RenderState* src, RenderState* dst) {
    memcpy(dst, src, sizeof(RenderState));
}

void SetViewMatrix (RenderState* rs, mat4 view, mat4 viewInv, mat4 viewLast) {
    glm_mat4_copy(view,     rs->matView);
    glm_mat4_copy(viewInv,  rs->matViewInv);
    glm_mat4_copy(viewLast, rs->matViewLast);
}

void SetProjMatrix (RenderState* rs, mat4 proj, mat4 projInv, mat4 projLast) {
    glm_mat4_copy(proj,     rs->matProj);
    glm_mat4_copy(projInv,  rs->matProjInv);
    glm_mat4_copy(projLast, rs->matProjLast);
}

void SetCamera (RenderState* rs, Camera* cam) {
    SetViewMatrix(rs, cam->view_matrix, cam->inv_view_matrix, cam->last_view_matrix);
    SetProjMatrix(rs, cam->proj_matrix, cam->inv_proj_matrix, cam->last_proj_matrix);
}

void ResetModelMatrix (RenderState* rs) {
    glm_mat4_identity(rs->matModel);
    glm_mat4_identity(rs->matModelLast);
}

void SetModelMatrix (RenderState* rs, mat4 model, mat4 modelLast) {
    glm_mat4_copy(model,     rs->matModel);
    glm_mat4_copy(modelLast, rs->matModelLast);
}

void MulModelMatrix (RenderState* rs, mat4 model, mat4 modelLast) {
    // FIXME: correct order?
    glm_mat4_mul(rs->matModel,     model,     rs->matModel);
    glm_mat4_mul(rs->matModelLast, modelLast, rs->matModelLast);
}

void MulModelPosition (RenderState* rs, vec3 pos, vec3 posLast) {
    // FIXME: maybe we should generate a translation matrix and flip the multiplication order?
    glm_translate(rs->matModel,     pos);
    glm_translate(rs->matModelLast, posLast);
}

void MulModelRotation (RenderState* rs, versor rot, versor rotLast) {
    // FIXME: maybe we should generate a rotation matrix and flip the multiplication order?
    glm_quat_rotate(rs->matModel, rot, rs->matModel);
    glm_quat_rotate(rs->matModelLast, rotLast, rs->matModelLast);
}

void MulModelScale (RenderState* rs, vec3 scl, vec3 sclLast) {
    // FIXME: maybe we should generate a scale matrix and flip the multiplication order?
    glm_scale(rs->matModel,     scl);
    glm_scale(rs->matModelLast, sclLast);
}

// Binds a texture and sampler to an automatically-selected texture unit.
// Returns the texture unit index (ranging from 0 to vxglMaxTextureUnits-1), or -1 if no units are available.
// Note that the texture and sampler can be 0. See the OpenGL documentation for glBindTexture/glBindSampler.
// The [target] parameter should be set to e.g. GL_TEXTURE_2D or GL_TEXTURE_CUBE_MAP.
int BindTexture (RenderState* rs, GLenum target, GLuint texture, GLuint sampler) {
    if (rs->nextFreeTextureUnit >= vxglMaxTextureUnits) {
        vxLog("Warning: Texture unit limit (%u) reached, not binding texture %u", vxglMaxTextureUnits, texture);
        return -1;
    }
    int unit = rs->nextFreeTextureUnit++;
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(target, texture);
    glBindSampler(unit, sampler);
    return unit;
}

// Binds a 2D texture and sampler to a given program uniform. Invalid uniforms (-1) are ignored.
// Note that the sampler can be set to 0 in order to use the texture's default sampling parameters.
void SetUniformTextureSampler2D (RenderState* rs, GLint unif, GLuint tex, GLuint sampler) {
    if (unif != -1) {
        int unit = BindTexture(rs, GL_TEXTURE_2D, tex, sampler);
        if (unit != -1) {
            glUniform1i(unif, unit);
        }
    }
}

// Binds a 2D texture to a given program uniform. Invalid uniforms (-1) are ignored.
// A sampler is automatically configured from the given sampling parameters.
void SetUniformTexture2D (RenderState* rs, GLint unif, GLuint tex, GLenum min, GLenum mag, GLenum wrap) {
    if (unif != -1) {
        GLuint sampler = VXGL_SAMPLER[rs->nextFreeTextureUnit];
        glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, min);
        glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, mag);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, wrap);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, wrap);
        SetUniformTextureSampler2D(rs, unif, tex, sampler);
    }
}

// Binds a cubemap texture and sampler to a given program uniform. Invalid uniforms (-1) are ignored.
// Note that the sampler can be set to 0 in order to use the texture's default sampling parameters.
void SetUniformTextureSamplerCube (RenderState* rs, GLint unif, GLuint tex, GLuint sampler) {
    if (unif != -1) {
        int unit = BindTexture(rs, GL_TEXTURE_CUBE_MAP, tex, sampler);
        if (unit != -1) {
            glUniform1i(unif, unit);
        }
    }
}

// Binds a cubemap texture to a given program uniform. Invalid uniforms (-1) are ignored.
// A sampler is automatically configured from the given sampling parameters.
void SetUniformTextureCube (RenderState* rs, GLint unif, GLuint tex, GLenum min, GLenum mag, GLenum wrap) {
    if (unif != -1) {
        GLuint sampler = VXGL_SAMPLER[rs->nextFreeTextureUnit];
        glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, min);
        glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, mag);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, wrap);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, wrap);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, wrap);
        SetUniformTextureSamplerCube(rs, unif, tex, sampler);
    }
}

void SetRenderProgram (RenderState* rs, Program* p) {
    rs->program = p->object;
    
    // Reset next TU and uniform locations:
    rs->nextFreeTextureUnit = 0;
    #define X(name, glslName) name = -1;
    XM_PROGRAM_UNIFORMS
    #undef X

    if (p->object != 0) {
        glUseProgram(p->object);

        // Retrieve uniform locations:
        #define X(name, glslName) name = glGetUniformLocation(p->object, glslName);
        XM_PROGRAM_UNIFORMS
        #undef X

        // Set render target uniforms.
        // Doing it here is a bit weird, but I can't think of any other good options.
        // NOTE: Envmap render targets are only used when rendering the envmap and should never be bound as textures.
        #define X(name, format) SetUniformTextureSampler2D(rs, UNIF_ ## name, name, SMP_LINEAR);
        XM_RENDERTARGETS_SCREEN
        XM_RENDERTARGETS_SHADOW
        #undef X
    } else {
        vxLog("Warning: program (%s, %s) is not available", p->vsh->path, p->fsh->path);
    }
}

void SetRenderMaterial (RenderState* rs, Material* mat) {
    if (mat != rs->material) {
        rs->material = mat;

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
        glUniform1f(UNIF_CONST_OCCLUSION,  mat->const_occlusion);
        glUniform1f(UNIF_CONST_METALLIC,   mat->const_metallic);
        glUniform1f(UNIF_CONST_ROUGHNESS,  mat->const_roughness);
        SetUniformTextureSampler2D(rs, UNIF_TEX_DIFFUSE,      mat->tex_diffuse,       mat->smp_diffuse);
        SetUniformTextureSampler2D(rs, UNIF_TEX_OCC_MET_RGH,  mat->tex_occ_met_rgh,   mat->smp_occ_met_rgh);
        SetUniformTextureSampler2D(rs, UNIF_TEX_OCCLUSION,    mat->tex_occlusion,     mat->smp_occlusion);
        SetUniformTextureSampler2D(rs, UNIF_TEX_METALLIC,     mat->tex_metallic,      mat->smp_metallic);
        SetUniformTextureSampler2D(rs, UNIF_TEX_ROUGHNESS,    mat->tex_roughness,     mat->smp_roughness);
        SetUniformTextureSampler2D(rs, UNIF_TEX_NORMAL,       mat->tex_normal,        mat->smp_normal);
    }
}

void RenderMesh (RenderState* rs, vxConfig* conf, vxFrame* frame, Mesh* mesh, Material* material) {
    if (!mesh->gl_vertex_array || !mesh->gl_element_array) {
        vxLog("Warning: mesh 0x%lx has no VAO (%u) or EBO (%u)", mesh, mesh->gl_vertex_array, mesh->gl_element_array);
        return;
    }

    int saved_nextFreeTextureUnit = rs->nextFreeTextureUnit;
    Material* saved_material = rs->material;

    SetRenderMaterial(rs, material);
    glUniformMatrix4fv(UNIF_MODEL_MATRIX,      1, false, (float*) rs->matModel);
    glUniformMatrix4fv(UNIF_LAST_MODEL_MATRIX, 1, false, (float*) rs->matModelLast);
    glUniformMatrix4fv(UNIF_PROJ_MATRIX,       1, false, (float*) rs->matProj);
    glUniformMatrix4fv(UNIF_INV_PROJ_MATRIX,   1, false, (float*) rs->matProjInv);
    glUniformMatrix4fv(UNIF_LAST_PROJ_MATRIX,  1, false, (float*) rs->matProjLast);
    glUniformMatrix4fv(UNIF_VIEW_MATRIX,       1, false, (float*) rs->matView);
    glUniformMatrix4fv(UNIF_INV_VIEW_MATRIX,   1, false, (float*) rs->matViewInv);
    glUniformMatrix4fv(UNIF_LAST_VIEW_MATRIX,  1, false, (float*) rs->matViewLast);
    glUniform2f(UNIF_IRESOLUTION, (float) conf->displayW, (float) conf->displayH);
    glUniform1f(UNIF_ITIME,   frame->t);
    glUniform1ui(UNIF_IFRAME, frame->n);

    glBindVertexArray(mesh->gl_vertex_array);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->gl_element_array);

    GLsizei elementCount = mesh->gl_element_count * FAccessorComponentCount(mesh->gl_element_type);
    GLenum componentType = 0;
    size_t triangleCount = 0;
    switch (mesh->gl_element_type) {
        case FACCESSOR_UINT8:       { componentType = GL_UNSIGNED_BYTE;  triangleCount = elementCount / 3; break; }
        case FACCESSOR_UINT8_VEC3:  { componentType = GL_UNSIGNED_BYTE;  triangleCount = elementCount;     break; }
        case FACCESSOR_UINT16:      { componentType = GL_UNSIGNED_SHORT; triangleCount = elementCount / 3; break; }
        case FACCESSOR_UINT16_VEC3: { componentType = GL_UNSIGNED_SHORT; triangleCount = elementCount;     break; }
        case FACCESSOR_UINT32:      { componentType = GL_UNSIGNED_INT;   triangleCount = elementCount / 3; break; }
        case FACCESSOR_UINT32_VEC3: { componentType = GL_UNSIGNED_INT;   triangleCount = elementCount;     break; }
        default: { vxLog("Warning: Mesh 0x%lx has unknown index accessor type %ju", mesh, mesh->gl_element_type); }
    }

    if (componentType != 0) {
        glDrawElements(mesh->type, elementCount, componentType, NULL);
        pfFrameDrawCount += 1;
        pfFrameTriCount  += triangleCount;
        pfFrameVertCount += mesh->gl_vertex_count;
    }

    rs->nextFreeTextureUnit = saved_nextFreeTextureUnit;
    rs->material = saved_material;
}

void RenderModel (RenderState* rs, vxConfig* conf, vxFrame* frame, Model* model) {
    for (size_t i = 0; i < model->meshCount; i++) {
        RenderState rsMesh = *rs;
        MulModelMatrix(&rsMesh, model->meshTransforms[i], model->meshTransforms[i]);
        RenderMesh(&rsMesh, conf, frame, &model->meshes[i], model->meshMaterials[i]);
    }
}

#if 0
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
    mat4 inv_mat_view;
    mat4 inv_mat_proj;
    mat4 last_mat_view;
    mat4 last_mat_proj;
    mat4 mat_model;
    mat4 last_mat_model;
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
    Shader* shader = vxAlloc(1, Shader);
    shader->name = name;
    char* code = vxReadFile(path, "r", NULL);
    shader->full = code;
    if (strncmp(code, "#version", 8) == 0) {
        char* newline = strchr(code, '\n');
        shader->body = newline + 1;
        size_t linesize = (size_t)(newline - code + 1);
        shader->version = vxAlloc(linesize + 1, char);
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
        char* log = vxAlloc(logsize, char);
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
        char* log = vxAlloc(logsize, char);
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
    XM_RENDERTARGETS_SCREEN
    XM_RENDERTARGETS_SHADOW
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

void SetViewMatrix (mat4 mat, mat4 inv, mat4 last) {
    glm_mat4_copy(mat, S_RenderState.mat_view);
    glm_mat4_copy(inv, S_RenderState.inv_mat_view);
    glm_mat4_copy(last, S_RenderState.last_mat_view);
}
void SetProjMatrix (mat4 mat, mat4 inv, mat4 last) {
    glm_mat4_copy(mat, S_RenderState.mat_proj);
    glm_mat4_copy(inv, S_RenderState.inv_mat_proj);
    glm_mat4_copy(last, S_RenderState.last_mat_proj);
}
void SetModelMatrix (mat4 mmat, mat4 last) {
    glm_mat4_copy(mmat, S_RenderState.mat_model);
    glm_mat4_copy(last, S_RenderState.last_mat_model);
}
void AddModelMatrix (mat4 mmat, mat4 last) {
    glm_mat4_mul(S_RenderState.mat_model, mmat, S_RenderState.mat_model);
    glm_mat4_mul(S_RenderState.last_mat_model, last, S_RenderState.last_mat_model);
}
void AddModelPosition (vec3 position, vec3 last) {
    glm_translate(S_RenderState.mat_model, position);
    glm_translate(S_RenderState.last_mat_model, last);
}
void AddModelRotation (vec4 rotation, vec4 last) {
    glm_quat_rotate(S_RenderState.mat_model, rotation, S_RenderState.mat_model);
    glm_quat_rotate(S_RenderState.last_mat_model, last, S_RenderState.last_mat_model);
}
void AddModelScale (vec3 scale, vec3 last) {
    glm_scale(S_RenderState.mat_model, scale);
    glm_scale(S_RenderState.last_mat_model, last);
}
void ResetModelMatrix() {
    glm_mat4_identity(S_RenderState.mat_model);
    glm_mat4_identity(S_RenderState.last_mat_model);
}

void ResetMatrices() {
    glm_mat4_identity(S_RenderState.mat_view);
    glm_mat4_identity(S_RenderState.mat_proj);
    glm_mat4_identity(S_RenderState.last_mat_view);
    glm_mat4_identity(S_RenderState.last_mat_proj);
    ResetModelMatrix();
}

void SetCameraMatrices (Camera* cam) {
    SetViewMatrix(cam->view_matrix, cam->inv_view_matrix, cam->last_view_matrix);
    SetProjMatrix(cam->proj_matrix, cam->inv_proj_matrix, cam->last_proj_matrix);
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

void SetUniformCubemap (GLint uniform, GLuint texture, GLuint min, GLuint mag, GLuint wrap) {
    if (uniform != -1 && texture != 0) {
        GLuint tu = S_RenderState.next_texture_unit;
        GLuint sampler = VXGL_SAMPLER[tu];
        glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, min);
        glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, mag);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, wrap);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, wrap);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, wrap);
        glActiveTexture(GL_TEXTURE0 + tu);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
        glBindSampler(tu, sampler);
        glUniform1i(uniform, tu);
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
    glUniform1f(UNIF_CONST_OCCLUSION,  mat->const_occlusion);
    glUniform1f(UNIF_CONST_METALLIC,   mat->const_metallic);
    glUniform1f(UNIF_CONST_ROUGHNESS,  mat->const_roughness);
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
        glUniformMatrix4fv(UNIF_MODEL_MATRIX, 1, false, (float*) S_RenderState.mat_model);
        glUniformMatrix4fv(UNIF_PROJ_MATRIX, 1, false, (float*) S_RenderState.mat_proj);
        glUniformMatrix4fv(UNIF_VIEW_MATRIX, 1, false, (float*) S_RenderState.mat_view);
        glUniformMatrix4fv(UNIF_INV_PROJ_MATRIX, 1, false, (float*) S_RenderState.inv_mat_proj);
        glUniformMatrix4fv(UNIF_INV_VIEW_MATRIX, 1, false, (float*) S_RenderState.inv_mat_view);
        glUniformMatrix4fv(UNIF_LAST_MODEL_MATRIX, 1, false, (float*) S_RenderState.last_mat_model);
        glUniformMatrix4fv(UNIF_LAST_PROJ_MATRIX,  1, false, (float*) S_RenderState.last_mat_proj);
        glUniformMatrix4fv(UNIF_LAST_VIEW_MATRIX,  1, false, (float*) S_RenderState.last_mat_view);
        glUniform2f(UNIF_IRESOLUTION, (float) w, (float) h);
        glUniform1f(UNIF_ITIME, t);
        glUniform1ui(UNIF_IFRAME, frame);
        SetMaterial(mesh->material);
        glBindVertexArray(mesh->gl_vertex_array);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->gl_element_array);

        GLsizei elementCount = mesh->gl_element_count * FAccessorComponentCount(mesh->gl_element_type);
        GLenum componentType = 0;
        size_t triangleCount = 0;
        switch (mesh->gl_element_type) {
            case FACCESSOR_UINT8:       { componentType = GL_UNSIGNED_BYTE;  triangleCount = elementCount / 3; break; }
            case FACCESSOR_UINT8_VEC3:  { componentType = GL_UNSIGNED_BYTE;  triangleCount = elementCount;     break; }
            case FACCESSOR_UINT16:      { componentType = GL_UNSIGNED_SHORT; triangleCount = elementCount / 3; break; }
            case FACCESSOR_UINT16_VEC3: { componentType = GL_UNSIGNED_SHORT; triangleCount = elementCount;     break; }
            case FACCESSOR_UINT32:      { componentType = GL_UNSIGNED_INT;   triangleCount = elementCount / 3; break; }
            case FACCESSOR_UINT32_VEC3: { componentType = GL_UNSIGNED_INT;   triangleCount = elementCount;     break; }
            default: { vxLog("Warning: Mesh 0x%lx has unknown index accessor type %ju", mesh, mesh->gl_element_type); }
        }
        if (componentType != 0) {
            glDrawElements(mesh->type, elementCount, componentType, NULL);
            pfFrameDrawCount += 1;
            pfFrameTriCount  += triangleCount;
            pfFrameVertCount += mesh->gl_vertex_count;
        }
    }
}

void RenderModel (Model* model, int w, int h, float t, uint32_t frame) {
    for (size_t i = 0; i < model->meshCount; i++) {
        PushRenderState();
        AddModelMatrix(model->meshTransforms[i], model->meshTransforms[i]);
        RenderMesh(&model->meshes[i], w, h, t, frame);
        PopRenderState();
    }
}

// Executes a full-screen pass using the current vertex and fragment shader.
// NOTE: The vertex shader should be set to VSH_FULLSCREEN_PASS before running this pass.
void RunFullscreenPass (int w, int h, float t, uint32_t frame) {
    static GLuint vao = 0;
    static GLuint ebo = 0;
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
    glUniformMatrix4fv(UNIF_PROJ_MATRIX, 1, false, (float*) S_RenderState.mat_proj);
    glUniformMatrix4fv(UNIF_VIEW_MATRIX, 1, false, (float*) S_RenderState.mat_view);
    glUniformMatrix4fv(UNIF_INV_PROJ_MATRIX, 1, false, (float*) S_RenderState.inv_mat_proj);
    glUniformMatrix4fv(UNIF_INV_VIEW_MATRIX, 1, false, (float*) S_RenderState.inv_mat_view);
    glUniformMatrix4fv(UNIF_LAST_PROJ_MATRIX, 1, false, (float*) S_RenderState.last_mat_proj);
    glUniformMatrix4fv(UNIF_LAST_VIEW_MATRIX, 1, false, (float*) S_RenderState.last_mat_view);
    glUniform2f(UNIF_IRESOLUTION, (float) w, (float) h);
    glUniform1f(UNIF_ITIME, t);
    glUniform1ui(UNIF_IFRAME, frame);
    glBindVertexArray(vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    vxglTextureBarrier(); // lets the shader both read & write to the same texture
    glDrawElements(GL_TRIANGLES, vxSize(triangles), GL_UNSIGNED_SHORT, NULL);
    pfFrameDrawCount++;
    pfFrameTriCount  += vxSize(triangles) / 3;
    pfFrameVertCount += vxSize(points) / 2;
}
#endif