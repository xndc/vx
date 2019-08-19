#pragma once
#include "common.h"
#include "assets.h"

#define X(name, _) extern GLuint name;
XM_ASSETS_TEXTURES
XM_ASSETS_RENDERTARGETS_SCREENSIZE
XM_ASSETS_RENDERTARGETS_SHADOWSIZE
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

// Resizes all render targets and framebuffers.
void UpdateFramebuffers (int width, int height, int shadowsize);

// Loads a texture from disk and uploads it to the GPU. Returns its OpenGL ID.
// The name doesn't matter, it's only displayed in this function's debug output.
GLuint LoadTextureFromDisk (const char* name, const char* path, bool mips);