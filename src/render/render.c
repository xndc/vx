#include "render.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

void APIENTRY vxglDummyTextureBarrier() {}
PFNGLTEXTUREBARRIERPROC vxglTextureBarrier = vxglDummyTextureBarrier;
int vxglMaxTextureUnits = 16; // resonable default, apparently getting GL_MAX_TEXTURE_IMAGE_UNITS can fail

Material MAT_FULLSCREEN_QUAD;
Material MAT_LIGHT_VOLUME;
Material MAT_DIFFUSE_WHITE;
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
        vxLog("Warning: this program requires the GL_*_texture_barrier extension.");
    }

    // Generate standard materials:
    InitMaterial(&MAT_FULLSCREEN_QUAD);
    MAT_FULLSCREEN_QUAD.depth_test = false;
    InitMaterial(&MAT_LIGHT_VOLUME);
    MAT_LIGHT_VOLUME.depth_test = true;
    MAT_LIGHT_VOLUME.depth_write = false;
    MAT_LIGHT_VOLUME.depth_func = GL_LESS;
    MAT_LIGHT_VOLUME.cull = false;
    MAT_LIGHT_VOLUME.cull_face = GL_BACK;
    MAT_LIGHT_VOLUME.blend = true;
    MAT_LIGHT_VOLUME.blend_srcf = GL_ONE;
    MAT_LIGHT_VOLUME.blend_dstf = GL_ONE;
    InitMaterial(&MAT_DIFFUSE_WHITE);
    MAT_DIFFUSE_WHITE.const_metallic = 0.0f;

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

// Applies any new global OpenGL configuration values. Should be run at the start of each frame.
void StartFrame (vxConfig* conf, GLFWwindow* window) {
    // VSync setup:
    static int swapInterval = -100;
    if (swapInterval != conf->swapInterval) {
        swapInterval = conf->swapInterval;
        // Use adaptive VSync (swap interval -1) on machines that support it, and use regular VSync otherwise:
        if (glfwExtensionSupported("WGL_EXT_swap_control_tear") ||
            glfwExtensionSupported("GLX_EXT_swap_control_tear")) {
            glfwSwapInterval(swapInterval);
        } else {
            glfwSwapInterval((swapInterval < 0) ? 1 : swapInterval);
        }
    }

    // Clip-control setup for reverse depth:
    static bool lastClipControlState = false;
    if (conf->gpuSupportsClipControl != lastClipControlState) {
        lastClipControlState = conf->gpuSupportsClipControl;
        if (conf->gpuSupportsClipControl) {
            if (glfwExtensionSupported("GL_ARB_clip_control")) {
                glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
            } else if (glfwExtensionSupported("NV_depth_buffer_float")) {
                glDepthRangedNV(-1.0f, 1.0f);
            }
        } else {
            if (glfwExtensionSupported("GL_ARB_clip_control")) {
                glClipControl(GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE);
            } else if (glfwExtensionSupported("NV_depth_buffer_float")) {
                glDepthRangedNV(0.0f, 1.0f);
            }
        }
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
    rs->forceNoDepthTest = false;
    rs->forceNoDepthTest = false;

    // Reset OpenGL state as well:
    // Avoids issues like the shadow framebuffer not being cleared because we disable depth writes at some point.
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    StartGPUBlock(passName);
}

void EndRenderPass() {
    EndGPUBlock();
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
    // TODO: Compute these in Camera_Update() instead of here:
    glm_mat4_mul(cam->proj_matrix,      cam->view_matrix,       rs->matVP);
    glm_mat4_mul(cam->inv_view_matrix,  cam->inv_proj_matrix,   rs->matVPInv);
    glm_mat4_mul(cam->last_proj_matrix, cam->last_view_matrix,  rs->matVPLast);
    // Determine camera position from the matrices, assuming a perspective projection.
    mat4 viewInvLast;
    glm_mat4_inv(cam->last_view_matrix, viewInvLast);
    vec4 selector = {0, 0, 0, 1}; // selects translation
    vec4 camPos, camPosLast;
    glm_mat4_mulv(rs->matViewInv, selector, camPos);
    glm_mat4_mulv(viewInvLast, selector, camPosLast);
    glm_vec3_copy(camPos, rs->camPos);
    glm_vec3_copy(camPosLast, rs->camPosLast);
}

void ResetModelMatrix (RenderState* rs) {
    glm_mat4_identity(rs->matModel);
    glm_mat4_identity(rs->matModelLast);
    glm_mat4_copy(rs->matVP,     rs->matMVP);
    glm_mat4_copy(rs->matVPLast, rs->matMVPLast);
}

// WARNING: This should be run after SetCamera()!
void SetModelMatrix (RenderState* rs, mat4 model, mat4 modelLast) {
    glm_mat4_copy(model,     rs->matModel);
    glm_mat4_copy(modelLast, rs->matModelLast);
    glm_mat4_mul(rs->matVP,     model,     rs->matMVP);
    glm_mat4_mul(rs->matVPLast, modelLast, rs->matMVPLast);
}

void MulModelMatrix (RenderState* rs, mat4 model, mat4 modelLast) {
    // FIXME: correct order?
    glm_mat4_mul(rs->matModel,     model,     rs->matModel);
    glm_mat4_mul(rs->matModelLast, modelLast, rs->matModelLast);
    glm_mat4_mul(rs->matMVP,       model,     rs->matMVP);
    glm_mat4_mul(rs->matMVPLast,   modelLast, rs->matMVPLast);
}

void MulModelPosition (RenderState* rs, vec3 pos, vec3 posLast) {
    // FIXME: maybe we should generate a translation matrix and flip the multiplication order?
    glm_translate(rs->matModel,     pos);
    glm_translate(rs->matModelLast, posLast);
    glm_mat4_mul(rs->matVP,     rs->matModel,     rs->matMVP);
    glm_mat4_mul(rs->matVPLast, rs->matModelLast, rs->matMVPLast);
}

void MulModelRotation (RenderState* rs, versor rot, versor rotLast) {
    // FIXME: maybe we should generate a rotation matrix and flip the multiplication order?
    glm_quat_rotate(rs->matModel, rot, rs->matModel);
    glm_quat_rotate(rs->matModelLast, rotLast, rs->matModelLast);
    glm_mat4_mul(rs->matVP,     rs->matModel,     rs->matMVP);
    glm_mat4_mul(rs->matVPLast, rs->matModelLast, rs->matMVPLast);
}

void MulModelScale (RenderState* rs, vec3 scl, vec3 sclLast) {
    // FIXME: maybe we should generate a scale matrix and flip the multiplication order?
    glm_scale(rs->matModel,     scl);
    glm_scale(rs->matModelLast, sclLast);
    glm_mat4_mul(rs->matVP,     rs->matModel,     rs->matMVP);
    glm_mat4_mul(rs->matVPLast, rs->matModelLast, rs->matMVPLast);
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
            glBlendEquation(mat->blend_func);
        } else {
            glDisable(GL_BLEND);
        }

        if (mat->cull) {
            glEnable(GL_CULL_FACE);
            if (rs->forceCullFace == 0) {
                glCullFace(mat->cull_face);
            } else {
                glCullFace(rs->forceCullFace);
            }
        } else {
            glDisable(GL_CULL_FACE);
        }

        if (mat->depth_test && !rs->forceNoDepthTest) {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(mat->depth_func);
            glDepthMask((mat->depth_write && !rs->forceNoDepthWrite) ? GL_TRUE : GL_FALSE);
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
        SetUniformTextureSampler2D(rs, UNIF_TEX_OCC_RGH_MET,  mat->tex_occ_rgh_met,   mat->smp_occ_rgh_met);
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

    glUniformMatrix4fv(UNIF_MVP,      1, false, (float*) rs->matMVP);
    glUniformMatrix4fv(UNIF_MVP_LAST, 1, false, (float*) rs->matMVPLast);
    glUniformMatrix4fv(UNIF_VP,       1, false, (float*) rs->matVP);
    glUniformMatrix4fv(UNIF_VP_INV,   1, false, (float*) rs->matVPInv);
    glUniformMatrix4fv(UNIF_VP_LAST,  1, false, (float*) rs->matVPLast);

    glUniform3fv(UNIF_CAMERA_POS,      1, (float*) rs->camPos);
    glUniform3fv(UNIF_CAMERA_POS_LAST, 1, (float*) rs->camPosLast);

    glUniform2i(UNIF_IRESOLUTION, conf->displayW, conf->displayH);
    glUniform1f(UNIF_ITIME,  frame->t);
    glUniform1i(UNIF_IFRAME, (int) frame->n);

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
        frame->perfDrawCalls += 1;
        frame->perfTriangles += triangleCount;
        frame->perfVertices += mesh->gl_vertex_count;
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