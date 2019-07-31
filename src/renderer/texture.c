#include "texture.h"
#include <glad/glad.h>
#include <stb/stb_image.h>
#include <flib/accessor.h>

typedef struct {
    char* buffer;
    bool depth;
} TextureCacheKey;

typedef struct {
    TextureCacheKey key;
    Texture value;
} TextureCacheEntry;

typedef struct {
    Sampler key; // with gl_sampler=0
    GLuint value;
} SamplerCacheEntry;

TextureCacheEntry* S_TextureCache;
SamplerCacheEntry* S_SamplerCache;

Texture* TextureFromFile (const char* path, bool depth) {
    Texture* tex;
    TextureCacheKey k;
    size_t buffer_size;
    k.buffer = FBufferFromFile(path, &buffer_size);
    k.depth = depth;
    ptrdiff_t i = hmgeti(S_TextureCache, k);
    if (i >= 0) {
        tex = &S_TextureCache[i].value;
    } else {
        Texture t;
        int readw, readh, readchan;
        char* image = (char*) stbi_load_from_memory((void*) k.buffer, (int) buffer_size,
            &readw, &readh, &readchan, 0);
        if (!image) {
            VXPANIC("Failed to load texture %s: %s", path, stbi_failure_reason());
        }
        t.path = vxStringDuplicate(path);
        t.mem = image;
        t.w = (uint32_t) readw;
        t.h = (uint32_t) readh;
        t.chan = (uint8_t) readchan;
        t.depth = depth;
        t.gl_id = 0;
        t.gpu_only = false;
        hmput(S_TextureCache, k, t);
        ptrdiff_t inew = hmgeti(S_TextureCache, k);
        tex = &S_TextureCache[inew].value;
        VXINFO("Read texture %s (0x%jx, index %jd) into 0x%jx (%dx%d, %d channels, buffer 0x%jx)",
            path, tex, inew, image, readw, readh, readchan, k.buffer);
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