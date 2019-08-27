#pragma once
#include "common.h"
#include "main.h"
#include "data/texture.h"
#include "data/model.h"
#include "data/camera.h"
#include "render/program.h"

typedef struct GLFWwindow GLFWwindow;

extern PFNGLTEXTUREBARRIERPROC vxglTextureBarrier;
extern int vxglMaxTextureUnits;

extern Material MAT_FULLSCREEN_QUAD;
extern Material MAT_DIFFUSE_WHITE;
extern Mesh MESH_QUAD;
extern Mesh MESH_CUBE;

void InitRenderSystem();
void StartFrame (vxConfig* conf, GLFWwindow* window);

typedef struct RenderState {
    GLuint program;
    Material* material;
    mat4 matView;
    mat4 matViewInv;
    mat4 matViewLast;
    mat4 matProj;
    mat4 matProjInv;
    mat4 matProjLast;
    mat4 matModel;
    mat4 matModelLast;
    int nextFreeTextureUnit;
    bool forceNoDepthTest;
    bool forceNoDepthWrite;
} RenderState;

void StartRenderPass (RenderState* rs, const char* passName);
void EndRenderPass();

#define RenderPass(pRenderState, passName, block) { StartRenderPass(pRenderState, passName); block; EndRenderPass(); }

void CopyRenderState (RenderState* src, RenderState* dst);

void SetViewMatrix (RenderState* rs, mat4 view, mat4 viewInv, mat4 viewLast);
void SetProjMatrix (RenderState* rs, mat4 proj, mat4 projInv, mat4 projLast);
void SetCamera (RenderState* rs, Camera* cam);

void ResetModelMatrix (RenderState* rs);
void SetModelMatrix (RenderState* rs, mat4 model, mat4 modelLast);
void MulModelMatrix (RenderState* rs, mat4 model, mat4 modelLast);
void MulModelPosition (RenderState* rs, vec3 pos, vec3 posLast);
void MulModelRotation (RenderState* rs, versor rot, versor rotLast);
void MulModelScale (RenderState* rs, vec3 scl, vec3 sclLast);

int BindTexture (RenderState* rs, GLenum target, GLuint texture, GLuint sampler);
void SetUniformTextureSampler2D (RenderState* rs, GLint unif, GLuint tex, GLuint sampler);
void SetUniformTexture2D (RenderState* rs, GLint unif, GLuint tex, GLenum min, GLenum mag, GLenum wrap);
void SetUniformTextureSamplerCube (RenderState* rs, GLint unif, GLuint tex, GLuint sampler);
void SetUniformTextureCube (RenderState* rs, GLint unif, GLuint tex, GLenum min, GLenum mag, GLenum wrap);

void SetRenderProgram (RenderState* rs, Program* p);
void SetRenderMaterial (RenderState* rs, Material* mat);

void RenderMesh  (RenderState* rs, vxConfig* conf, vxFrame* frame, Mesh* mesh, Material* material);
void RenderModel (RenderState* rs, vxConfig* conf, vxFrame* frame, Model* model);

#if 0
// The process of rendering something in VX looks something like this:
// * Start a new render pass.
// * Bind whichever framebuffer you want to render to.
// * Set the render program you want to use for the pass.
// * Set the view and projection matrices you want to use for the pass.
// Then, for each mesh you want to render:
// * Change the render program, if required.
// * Set the appropriate model matrix for the mesh.
// * Set any OpenGL state required to draw the mesh's material.
// * Set shader uniforms (colours, textures, etc.) as appropriate.
// * Issue a draw call for the mesh.

void StartRenderPass (const char* name);
void PushRenderState();
void PopRenderState();

void SetRenderVertShader (Shader* vsh);
void SetRenderFragShader (Shader* fsh);
void SetRenderProgram (Shader* vsh, Shader* fsh);

void AddShaderDefine (const char* name, const char* text);
void ResetShaderDefines();

void SetViewMatrix (mat4 mat, mat4 inv, mat4 last);
void SetProjMatrix (mat4 mat, mat4 inv, mat4 last);
void SetModelMatrix (mat4 mmat, mat4 last);
void GetModelMatrix (mat4 dest, mat4 last);
void AddModelMatrix (mat4 mmat, mat4 last);
void AddModelPosition (vec3 position, vec3 last);
void AddModelRotation (vec4 rotation, vec3 last);
void AddModelScale (vec3 scale, vec3 last);
void ResetModelMatrix();
void ResetMatrices();
void SetCameraMatrices (Camera* cam);

GLuint BindTextureUnit (GLuint texture, GLuint sampler);
void SetUniformTexture (GLint uniform, GLuint texture, GLuint min, GLuint mag, GLuint wrap);
void SetUniformCubemap (GLint uniform, GLuint texture, GLuint min, GLuint mag, GLuint wrap);
void SetUniformTexSampler (GLint uniform, GLuint texture, GLuint sampler);
void ResetShaderVariables();

void SetMaterial (Material* mat);
void RenderMesh  (Mesh* mesh,   int w, int h, float t, uint32_t frame);
void RenderModel (Model* model, int w, int h, float t, uint32_t frame);
void RunFullscreenPass         (int w, int h, float t, uint32_t frame);
#endif