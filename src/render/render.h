#pragma once
#include "common.h"
#include "assets.h"
#include "data/texture.h"
#include "data/model.h"
#include "data/camera.h"

// The process of rendering something in VX looks something like this:
// * Start a new render pass.
// * Bind whichever framebuffer you want to render to.
// * Set the vertex and fragment shaders that you want to use.
// * Add #define statements to the shaders, if needed.
// * Set the view and projection matrices you want to use for the pass.
// Then, for each mesh you want to render:
// * Change shaders or add more defines if required.
// * Set the appropriate model matrix for the mesh.
// * Set any OpenGL state required to draw the mesh's material.
// * Set shader uniforms (colours, textures, etc.) as appropriate.
// * Issue a draw call for the mesh.

typedef struct {
    const char* name;
    char* version;
    char* body;
    char* full;
} Shader;

#define X(name, path) extern Shader* name;
XM_ASSETS_SHADERS
#undef X

#define X(name, glsl_name) extern GLint name;
XM_ASSETS_SHADER_UNIFORMS
#undef X

#define X(name, location, glslname, gltfname) extern const GLint name;
XM_ATTRIBUTE_LOCATIONS
#undef X

extern PFNGLTEXTUREBARRIERPROC vxglTextureBarrier;

Shader* LoadShaderFromDisk (const char* name, const char* path);

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