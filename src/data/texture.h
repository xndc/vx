#pragma once
#include "common.h"
#include "assets.h"

#define X(name, _) extern GLuint name;
XM_ASSETS_TEXTURES
XM_RENDERTARGETS_SCREEN
XM_RENDERTARGETS_SHADOW
XM_RENDERTARGETS_ENVMAP
#undef X

extern GLuint ENVMAP_BASE;
extern GLuint ENVMAP_CUBE;

#define BEGIN(name) \
    extern GLuint name; \
    static const GLenum name ## _BUFFERS [] = {
#define DEPTH(point, name)
// #define COLOR(point, name) VX ## point ,
#define COLOR(point, name) point ,
#define END(name) \
    0 }; \
    static const size_t name ## _BUFFER_COUNT = vxSize(name ## _BUFFERS) - 1;
XM_ASSETS_FRAMEBUFFERS
#undef BEGIN
#undef DEPTH
#undef COLOR
#undef END

#define BindFramebuffer(name) \
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, name); \
    glDrawBuffers(name ## _BUFFER_COUNT, name ## _BUFFERS);

#define VXGL_SAMPLER_COUNT 32
extern GLuint VXGL_SAMPLER [VXGL_SAMPLER_COUNT];

void InitTextures();

// Resizes all render targets and framebuffers. Returns a bitfield containing UPDATED_*_TARGETS flags for each render
// target type that was resized.
int UpdateFramebuffers (int width, int height, int shadowsize, int envmapsize, int skyboxsize);
static const int UPDATED_SCREEN_TARGETS = 1 << 0;
static const int UPDATED_SHADOW_TARGETS = 1 << 1;
static const int UPDATED_ENVMAP_TARGETS = 1 << 2;
static const int UPDATED_SKYBOX_TARGETS = 1 << 3;

// Loads a texture from disk and uploads it to the GPU. Returns its OpenGL ID.
// The name doesn't matter, it's only displayed in this function's debug output.
GLuint LoadTextureFromDisk (const char* name, const char* path, bool mips);