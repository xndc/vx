#pragma once
#include "common.h"
#include "assets.h"
#include "data/texture.h"
#include "data/model.h"
#include "data/camera.h"
#include <cglm/cglm.h>

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

#define X(name, location, glsl_name) extern GLint name;
XM_ASSETS_SHADER_ATTRIBUTES
#undef X

// void InitRenderSystem();
Shader* LoadShaderFromDisk (const char* name, const char* path);

void StartRenderPass (const char* name);
void PushRenderState();
void PopRenderState();

void SetRenderVertShader (Shader* vsh);
void SetRenderFragShader (Shader* fsh);
void SetRenderProgram (Shader* vsh, Shader* fsh);

void AddShaderDefine (const char* name, const char* text);
void ResetShaderDefines();

void SetViewMatrix (mat4 vmat);
void SetProjMatrix (mat4 pmat);
void SetModelMatrix (mat4 mmat);
void GetModelMatrix (mat4 dest);
void AddModelMatrix (mat4 mmat);
void AddModelPosition (vec3 position);
void AddModelRotation (vec4 rotation);
void AddModelScale (vec3 scale);
void ResetModelMatrix();
void ResetMatrices();
void SetCameraMatrices (Camera* cam);

GLuint BindTextureUnit (GLuint texture, GLuint sampler);
void SetUniformTexture (GLint uniform, GLuint texture, GLuint min, GLuint mag, GLuint wrap);
void SetUniformTexSampler (GLint uniform, GLuint texture, GLuint sampler);
void ResetShaderVariables();

void SetMaterial (Material* mat);
void RenderMesh (Mesh* mesh);
void RenderModel (Model* model);

// Executes a full-screen pass using the current vertex and fragment shader.
// NOTE: The vertex shader should be set to VSH_FULLSCREEN_PASS before running this pass.
void RunFullscreenPass (int w, int h);