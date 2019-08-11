#pragma once
#include <common.h>
#include <flib/vec.h>

typedef struct {
    char* path;
    char* mem;
    uint32_t w;
    uint32_t h;
    uint8_t chan;
    GLuint gl_id;
} Texture;

// Loads a texture from a file, uploading it to the GPU if requested.
// Will return the same in-memory texture for each invocation with the same path.
Texture* GetTexture (const char* path, bool upload, bool gpu_only);

// Uploads a texture to the GPU.
void TextureUpload (Texture* texture);

typedef struct {
    GLuint gl_sampler;
    GLenum min_filter;
    GLenum mag_filter;
    GLenum wrap;
    GLenum depth_function; // 0 for non-shadow samplers
} Sampler;

Sampler GetColorSampler (GLenum min_filter, GLenum mag_filter, GLenum wrap);
Sampler GetShadowSampler (GLenum depth_function);