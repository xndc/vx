#pragma once
#include "common.h"
#include "assets.h"

#define X(name, _) extern GLuint name;
XM_ASSETS_TEXTURES
XM_ASSETS_RENDERTARGETS_SCREENSIZE
XM_ASSETS_RENDERTARGETS_SHADOWSIZE
#undef X

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

// Initializes the texture subsystem. Creates the following:
// * the textures, render targets and framebuffers defined in assets.h (TEX_*, RT_*, FB_*)
// * some sampler objects for the render system to use (VXGL_SAMPLER[i])
// void InitTextureSystem();
GLuint LoadTextureFromDisk (const char* name, const char* path);

// Resizes all render targets and framebuffers.
void UpdateFramebuffers (int width, int height, int shadowsize);

// Loads a texture from disk and uploads it to the GPU. Returns its OpenGL ID.
// The name doesn't matter, it's only displayed in this function's debug output.
GLuint LoadTextureFromDisk (const char* name, const char* path);