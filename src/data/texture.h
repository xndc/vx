#pragma once
#include "common.h"
#include "assets.h"

#define X(name, _) extern GLuint name;
XM_ASSETS_TEXTURES
XM_ASSETS_RENDERTARGETS_SCREENSIZE
XM_ASSETS_RENDERTARGETS_SHADOWSIZE
#undef X

#define VXGL_COLOR_ATTACHMENT0 (0x8CE0)
#define VXGL_COLOR_ATTACHMENT1 (VXGL_COLOR_ATTACHMENT0 + 1)
#define VXGL_COLOR_ATTACHMENT2 (VXGL_COLOR_ATTACHMENT0 + 2)
#define VXGL_COLOR_ATTACHMENT3 (VXGL_COLOR_ATTACHMENT0 + 3)
#define VXGL_COLOR_ATTACHMENT4 (VXGL_COLOR_ATTACHMENT0 + 4)
#define VXGL_COLOR_ATTACHMENT5 (VXGL_COLOR_ATTACHMENT0 + 5)
#define VXGL_COLOR_ATTACHMENT6 (VXGL_COLOR_ATTACHMENT0 + 6)
#define VXGL_COLOR_ATTACHMENT7 (VXGL_COLOR_ATTACHMENT0 + 7)
#define VXGL_COLOR_ATTACHMENT8 (VXGL_COLOR_ATTACHMENT0 + 8)

#define BEGIN(name) \
    extern GLuint name; \
    static const GLenum name ## _BUFFERS [] = {
#define DEPTH(point, name)
#define COLOR(point, name) VX ## point ,
#define END(name) \
    0 }; \
    static const size_t name ## _BUFFER_COUNT = vxSize(name ## _BUFFERS) - 1;
XM_ASSETS_FRAMEBUFFERS
#undef BEGIN
#undef DEPTH
#undef COLOR
#undef END

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