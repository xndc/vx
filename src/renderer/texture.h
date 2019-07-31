#pragma once
#include <common.h>
#include <flib/vec.h>

typedef struct {
    char* path;
    char* mem;
    uint32_t w;
    uint32_t h;
    uint8_t chan;
    bool depth;
    bool gpu_only;
    GLuint gl_id;
} Texture;

// Loads a texture from a file.
// Will return the same in-memory texture for each invocation with the same parameters.
Texture* TextureFromFile (const char* path, bool depth);

typedef struct {
    GLuint gl_sampler;
    GLenum min_filter;
    GLenum mag_filter;
    GLenum wrap;
    GLenum depth_function; // 0 for non-shadow samplers
} Sampler;

Sampler GetColorSampler (GLenum min_filter, GLenum mag_filter, GLenum wrap);
Sampler GetShadowSampler (GLenum depth_function);