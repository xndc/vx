#pragma once
#include "common.h"
#include "assets.h"
#include "main.h"

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
    static const GLsizei name ## _BUFFER_COUNT = vxSize(name ## _BUFFERS) - 1;
XM_FRAMEBUFFERS
#undef BEGIN
#undef DEPTH
#undef COLOR
#undef END

#define BindFramebuffer(name) \
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, name); \
    glDrawBuffers(name ## _BUFFER_COUNT, name ## _BUFFERS);

#define VXGL_SAMPLER_COUNT 32
extern GLuint VXGL_SAMPLER [VXGL_SAMPLER_COUNT];

extern GLuint TEX_WHITE_1x1;
extern GLuint SMP_NEAREST;
extern GLuint SMP_LINEAR;

void InitTextureSystem();
void LoadTextures();

uint8_t UpdateRenderTargets (vxConfig* conf);
static const int UPDATED_SCREEN_TARGETS = 1 << 0;
static const int UPDATED_SHADOW_TARGETS = 1 << 1;
static const int UPDATED_ENVMAP_TARGETS = 1 << 2;
static const int UPDATED_SKYBOX_TARGETS = 1 << 3;

GLuint LoadTextureFromDisk (const char* path, bool mips);