#include "texture.h"
#include <glad/glad.h>
#include <glfw/glfw3.h>
#include <stb/stb_image.h>

#define X(name, _) GLuint name = 0;
XM_ASSETS_TEXTURES
XM_ASSETS_RENDERTARGETS_SCREENSIZE
XM_ASSETS_RENDERTARGETS_SHADOWSIZE
#undef X

#define BEGIN(name) GLuint name = 0;
#define DEPTH(point, name)
#define COLOR(point, name)
#define END(name)
XM_ASSETS_FRAMEBUFFERS
#undef BEGIN
#undef DEPTH
#undef COLOR
#undef END

GLuint VXGL_SAMPLER [VXGL_SAMPLER_COUNT];

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
        #define DEPTH(point, name) \
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, point, GL_TEXTURE_2D, name, 0);
        #define COLOR(point, name) \
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, point, GL_TEXTURE_2D, name, 0);
        #define END(name) \
            status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER); \
            if (status != GL_FRAMEBUFFER_COMPLETE) { \
                VXPANIC("Couldn't create framebuffer " #name ": error %d", status); \
            }
        XM_ASSETS_FRAMEBUFFERS
        #undef BEGIN
        #undef DEPTH
        #undef COLOR
        #undef END
    }
}

GLuint LoadTextureFromDisk (const char* name, const char* path) {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Read from disk:
    int w, h, c;
    double tb_stbi_load = glfwGetTime();
    void* image = stbi_load(path, &w, &h, &c, 0);
    if (!image) {
        VXPANIC("Failed to load %s from %s: %s", name, path, stbi_failure_reason());
    }

    // Upload:
    double tb_upload = glfwGetTime();
    const char* type = "?";
    switch (c) {
        case 1: {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, image);
            type = "R8";
        } break;
        case 2: {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, w, h, 0, GL_RG, GL_UNSIGNED_BYTE, image);
            type = "RG8";
        } break;
        case 3: {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            type = "RGB8";
        } break;
        case 4: {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
            type = "RGBA8";
        } break;
        default: {
            VXWARN("Unknown channel count %d for %s", c, name);
        }
    }

    double tb_generate_mips = glfwGetTime();
    glGenerateMipmap(GL_TEXTURE_2D);

    double ta_end = glfwGetTime();
    double tt_stbi_load_ms = (tb_upload - tb_stbi_load) * 1000.0;
    double tt_upload_ms = (tb_generate_mips - tb_upload) * 1000.0;
    double tt_generate_mips_ms = (ta_end - tb_generate_mips) * 1000;
    VXINFO("Uploaded %s (object %d, %s, %dx%d) (load %.02f ms, upload %.02f ms, mips %.02f ms)",
        path, texture, type, w, h, tt_stbi_load_ms, tt_upload_ms, tt_generate_mips_ms);

    return texture;
}