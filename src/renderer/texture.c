#include "texture.h"
#include <glad/glad.h>
#include <stb/stb_image.h>
#include <flib/accessor.h>

typedef struct {
    char* key;
    Texture value;
} TextureCacheEntry;

typedef struct {
    Sampler key; // with gl_sampler=0
    GLuint value;
} SamplerCacheEntry;

TextureCacheEntry* S_TextureCache;
SamplerCacheEntry* S_SamplerCache;

Texture* GetTexture (const char* path, bool upload, bool gpu_only) {
    Texture* tex;
    size_t buffer_size;
    ptrdiff_t i = shgeti(S_TextureCache, path);
    if (i >= 0) {
        tex = &S_TextureCache[i].value;
    } else {
        Texture t;
        int readw, readh, readchan;
        void* image = stbi_load(path, &readw, &readh, &readchan, 0);
        if (!image) {
            VXPANIC("Failed to load texture %s: %s", path, stbi_failure_reason());
        }
        t.path = vxStringDuplicate(path);
        t.mem = image;
        t.w = (uint32_t) readw;
        t.h = (uint32_t) readh;
        t.chan = (uint8_t) readchan;
        t.gl_id = 0;
        shput(S_TextureCache, path, t);
        ptrdiff_t inew = shgeti(S_TextureCache, path);
        tex = &S_TextureCache[inew].value;
        VXINFO("Read texture %s (0x%jx, index %jd) into 0x%jx (%dx%d, %d channels)",
            path, tex, inew, image, readw, readh, t.chan);

        if (upload) {
            glGenTextures(1, &t.gl_id);
            glBindTexture(GL_TEXTURE_2D, t.gl_id);

            // Upload texture:
            if (t.chan == 1) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, t.w, t.h, 0, GL_RED, GL_UNSIGNED_BYTE, t.mem);
            } else if (t.chan == 2) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, t.w, t.h, 0, GL_RG, GL_UNSIGNED_BYTE, t.mem);
            } else if (t.chan == 3) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, t.w, t.h, 0, GL_RGB, GL_UNSIGNED_BYTE, t.mem);
            } else if (t.chan == 4) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, t.w, t.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, t.mem);
            } else {
                VXPANIC("Texture %s has %hu channels", t.path, t.chan);
            }
            glGenerateMipmap(GL_TEXTURE_2D); // TODO: mipmaps should be optional
            VXINFO("Uploaded texture to GPU as OpenGL texture %d", t.gl_id);

            if (gpu_only) {
                stbi_image_free(image);
                VXINFO("Freed texture image from memory (0x%jx)", image);
            }
        }
    }
    return tex;
}

Sampler GetColorSampler (GLenum min_filter, GLenum mag_filter, GLenum wrap) {
    Sampler smp = {0};
    smp.min_filter = min_filter;
    smp.mag_filter = mag_filter;
    smp.wrap = wrap;
    ptrdiff_t i = hmgeti(S_SamplerCache, smp);
    if (i >= 0) {
        smp.gl_sampler = S_SamplerCache[i].value;
    } else {
        GLuint s;
        glGenSamplers(1, &s);
        glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER, min_filter);
        glSamplerParameteri(s, GL_TEXTURE_MAG_FILTER, mag_filter);
        glSamplerParameteri(s, GL_TEXTURE_WRAP_S, wrap);
        glSamplerParameteri(s, GL_TEXTURE_WRAP_T, wrap);
        hmput(S_SamplerCache, smp, s);
        smp.gl_sampler = s;
        VXINFO("Generated OpenGL color sampler %d (minfilter %d, magfilter %d, wrap %d)",
            s, min_filter, mag_filter, wrap);
    }
    return smp;
}

Sampler GetDepthSampler (GLenum depth_function) {
    Sampler smp = {0};
    smp.depth_function = depth_function;
    ptrdiff_t i = hmgeti(S_SamplerCache, smp);
    if (i >= 0) {
        smp.gl_sampler = S_SamplerCache[i].value;
    } else {
        GLuint s;
        glGenSamplers(1, &s);
        glSamplerParameteri(s, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glSamplerParameteri(s, GL_TEXTURE_COMPARE_FUNC, depth_function);
        hmput(S_SamplerCache, smp, s);
        smp.gl_sampler = s;
        VXINFO("Generated OpenGL depth sampler %d (function %d)", depth_function);
    }
    return smp;
}