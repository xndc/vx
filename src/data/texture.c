#include "texture.h"
#include <glad/glad.h>
#include <stb/stb_image.h>

#define X(name, _) GLuint name = 0;
XM_ASSETS_TEXTURES
XM_ASSETS_RENDERTARGETS_SCREENSIZE
XM_ASSETS_RENDERTARGETS_SHADOWSIZE
#undef X

#define BEGIN(name) GLuint name = 0;
#define ATTACH(point, name)
#define END(name)
XM_ASSETS_FRAMEBUFFERS
#undef BEGIN
#undef ATTACH
#undef END

GLuint VXGL_SAMPLER [VXGL_SAMPLER_COUNT];

void InitTextureSystem() {
    // Create texture objects:
    #define X(name, _) glGenTextures(1, &name);
    XM_ASSETS_TEXTURES
    #undef X

    // Load textures from disk:
    #define X(name, path) name = LoadTextureFromDisk(#name, path);
    XM_ASSETS_TEXTURES
    #undef X

    // Generate samplers:
    glGenSamplers(VXGL_SAMPLER_COUNT, VXGL_SAMPLER);
}

void UpdateFramebuffers (int width, int height, int shadowsize) {
    static int last_width = 0;
    static int last_height = 0;
    static int last_shadowsize = 0;
    bool regenerate_framebuffers = false;

    // Resize screen-sized render targets:
    if (width != last_width || height != last_height) {
        regenerate_framebuffers = true;
        last_width  = width;
        last_height = height;
        #define X(name, format) \
            if (name != 0) { glDeleteTextures(1, &name); } \
            glGenTextures(1, &name); \
            glBindTexture(GL_TEXTURE_2D, name); \
            glTexStorage2D(GL_TEXTURE_2D, 1, format, width, height);
        XM_ASSETS_RENDERTARGETS_SCREENSIZE
        #undef X
    }

    if (shadowsize != last_shadowsize) {
        regenerate_framebuffers = true;
        last_shadowsize = shadowsize;
        // Resize shadow-sized render targets:
        #define X(name, format) \
            if (name != 0) { glDeleteTextures(1, &name); } \
            glGenTextures(1, &name); \
            glBindTexture(GL_TEXTURE_2D, name); \
            glTexStorage2D(GL_TEXTURE_2D, 1, format, shadowsize, shadowsize);
        XM_ASSETS_RENDERTARGETS_SHADOWSIZE
        #undef X
    }

    // Re-generate framebuffers:
    if (regenerate_framebuffers) {
        GLenum status;
        #define BEGIN(name) \
            if (name != 0) { glDeleteFramebuffers(1, &name); } \
            glGenFramebuffers(1, &name); \
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, name);
        #define ATTACH(point, name) \
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, point, GL_TEXTURE_2D, name, 0);
        #define END(name) \
            status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER); \
            if (status != GL_FRAMEBUFFER_COMPLETE) { \
                VXPANIC("Couldn't create framebuffer " #name ": error %d", status); \
            }
        XM_ASSETS_FRAMEBUFFERS
        #undef BEGIN
        #undef ATTACH
        #undef END
    }
}

GLuint LoadTextureFromDisk (const char* name, const char* path) {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Read from disk:
    int w, h, c;
    void* image = stbi_load(path, &w, &h, &c, 0);
    if (!image) {
        VXPANIC("Failed to load %s from %s: %s", name, path, stbi_failure_reason());
    }

    // Upload:
    switch (c) {
        case 1: {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, image);
            VXINFO("Uploaded %s (%s) to GPU (object %u, R8, %dx%d)", name, path, texture, w, h);
        } break;
        case 2: {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, w, h, 0, GL_RG, GL_UNSIGNED_BYTE, image);
            VXINFO("Uploaded %s (%s) to GPU (object %u, RG8, %dx%d)", name, path, texture, w, h);
        } break;
        case 3: {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            VXINFO("Uploaded %s (%s) to GPU (object %u, RGB8, %dx%d)", name, path, texture, w, h);
        } break;
        case 4: {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
            VXINFO("Uploaded %s (%s) to GPU (object %u, RGBA8, %dx%d)", name, path, texture, w, h);
        } break;
        default: {
            VXWARN("Unknown channel count %d for %s", c, name);
        }
    }
    glGenerateMipmap(GL_TEXTURE_2D);
}