#include "texture.h"
#include <glad/glad.h>
#include <glfw/glfw3.h>
#include <stb_image.h>

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

    // We get called with zeroes when the window is minimized. Just skip the update in this case.
    if (width == 0 || height == 0) {
        return;
    }

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
                vxPanic("Couldn't create framebuffer " #name ": error %d", status); \
            }
        XM_ASSETS_FRAMEBUFFERS
        #undef BEGIN
        #undef DEPTH
        #undef COLOR
        #undef END
    }
}

GLuint LoadTextureFromDisk (const char* name, const char* path, bool mips) {
    double tStart = glfwGetTime();
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Generate hash of texture filename and mtime:
    uint64_t mtime = vxGetFileMtime(path);
    vxCheckMsg(mtime != 0, "Failed to load %s from %s: file not found", name, path);
    size_t hash = stbds_hash_string(path, VX_SEED);
    hash ^= stbds_hash_bytes(&mtime, sizeof(mtime), VX_SEED);

    // Look for cached texture:
    static char cachedTexturePath [128];
    stbsp_snprintf(cachedTexturePath, 128, "userdata/texturecache/%jx.dat", hash);
    if (vxGetFileMtime(cachedTexturePath) != 0) {
        size_t size;
        char* data = vxReadFile(cachedTexturePath, "rb", &size);
        uint32_t w = ((uint32_t*) data)[0];
        uint32_t h = ((uint32_t*) data)[1];
        uint32_t c = ((uint32_t*) data)[2]; // channels
        uint32_t l = ((uint32_t*) data)[3]; // mip levels
        size_t idata = 4 * sizeof(uint32_t);
        GLenum internalformat, format;
        switch (c) {
            case 1: { internalformat = GL_R8;    format = GL_RED;  break; }
            case 2: { internalformat = GL_RG8;   format = GL_RG;   break; }
            case 3: { internalformat = GL_RGB8;  format = GL_RGB;  break; }
            case 4: { internalformat = GL_RGBA8; format = GL_RGBA; break; }
            default: { vxPanic("Unknown channel count %d for %s (%s)", c, path, cachedTexturePath); }
        }
        uint32_t levelw = w;
        uint32_t levelh = h;
        for (int ilevel = 0; ilevel < l; ilevel++) {
            if ((size - idata) < levelw * levelh * c) {
                vxPanic("%s has wrong size %ju for parameters %ux%ux%ux%u", size, w, h, c, l);
            }
            char* leveldata = data + idata;
            idata += levelw * levelh * c;
            glTexImage2D(GL_TEXTURE_2D, ilevel, internalformat, levelw, levelh, 0, format,
                GL_UNSIGNED_BYTE, leveldata);
            levelw /= 2;
            levelh /= 2;
        }
        free(data);
        double t = (glfwGetTime() - tStart) * 1000.0;
        vxLog("Read from cache: %s (FBO %u, %ux%ux%ux%u, %.02lf ms)", path, texture, w, h, c, l, t);
        return texture;
    }

    // Read from disk:
    int w, h, c;
    void* image = stbi_load(path, &w, &h, &c, 0);
    if (!image) {
        vxPanic("Failed to load %s from %s: %s", name, path, stbi_failure_reason());
    }

    // Compute mip level count (no way to query it):
    // https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_texture_non_power_of_two.txt
    uint32_t l = 1;
    if (mips) {
        l += (uint32_t) floor(log2(vxMax(vxMax(w, h), 2)));
    }

    // Upload:
    GLenum internalformat, format;
    switch (c) {
        case 1: { internalformat = GL_R8;    format = GL_RED;  break; }
        case 2: { internalformat = GL_RG8;   format = GL_RG;   break; }
        case 3: { internalformat = GL_RGB8;  format = GL_RGB;  break; }
        case 4: { internalformat = GL_RGBA8; format = GL_RGBA; break; }
        default: { vxPanic("Unknown channel count %d for %s (%s)", c, path, cachedTexturePath); }
    }
    glTexImage2D(GL_TEXTURE_2D, 0, internalformat, w, h, 0, format, GL_UNSIGNED_BYTE, image);
    if (mips) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    free(image);

    // Cache:
    vxCreateDirectory("userdata");
    vxCreateDirectory("userdata/texturecache");
    FILE* cachedTextureFile = fopen(cachedTexturePath, "wb");
    if (cachedTextureFile == 0) {
        vxLog("Warning: can't open %s for writing: %s", cachedTexturePath, strerror(errno));
    } else {
        uint32_t info[] = {(uint32_t) w, (uint32_t) h, (uint32_t) c, l};
        fwrite(info, sizeof(uint32_t), 4, cachedTextureFile);
        for (int ilevel = 0; ilevel < l; ilevel++) {
            // FIXME: the docs for glGetTexImage mention GL_PACK_ALIGNMENT - what's that?
            uint32_t levelw, levelh;
            glGetTexLevelParameteriv(GL_TEXTURE_2D, ilevel, GL_TEXTURE_WIDTH,  &levelw);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, ilevel, GL_TEXTURE_HEIGHT, &levelh);
            void* pixels = malloc((levelw + 1) * (levelh + 1) * c);
            glGetTexImage(GL_TEXTURE_2D, ilevel, format, GL_UNSIGNED_BYTE, pixels);
            fwrite(pixels, 1, levelw * levelh * c, cachedTextureFile);
            free(pixels);
        }
        fclose(cachedTextureFile);
    }

    double t = (glfwGetTime() - tStart) * 1000.0;
    vxLog("Read from disk: %s (FBO %u, %dx%dx%dx%u, %.02lf ms)", path, texture, w, h, c, l, t);

    return texture;
}
