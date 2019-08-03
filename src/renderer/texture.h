#pragma once
#include <common.h>
#include <assets.h>

#define X(name, _) extern GLuint name;
XM_ASSETS_TEXTURES
XM_ASSETS_RENDERTARGETS_SCREENSIZE
XM_ASSETS_RENDERTARGETS_SHADOWSIZE
#undef X

#define BEGIN_FRAMEBUFFER(name) extern GLuint name;
#define ATTACH(point, name)
#define END_FRAMEBUFFER(name)
XM_ASSETS_FRAMEBUFFERS
#undef BEGIN_FRAMEBUFFER
#undef ATTACH
#undef END_FRAMEBUFFER

#define VXGL_SAMPLER_COUNT 10
extern GLuint VXGL_SAMPLER [VXGL_SAMPLER_COUNT];

// Initializes the texture subsystem. Creates the following:
// * the textures, render targets and framebuffers defined in assets.h (TEX_*, RT_*, FB_*)
// * some sampler objects for the render system to use (VXGL_SAMPLER[i])
void InitTextureSystem();

// Resizes all render targets and framebuffers.
void UpdateFramebuffers (int width, int height, int shadowsize);