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
    mat4 matMVP;
    mat4 matMVPLast;
    mat4 matVP;
    mat4 matVPInv;
    mat4 matVPLast;
    vec3 camPos;
    vec3 camPosLast;
    int nextFreeTextureUnit;
    bool forceNoDepthTest;
    bool forceNoDepthWrite;
    GLenum forceCullFace;
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