#pragma once
#include <common.h>
#include <flib/accessor.h>
#include <flib/vec.h>
#include "texture.h"

typedef struct {
    bool blend;
    GLenum blend_srcf;
    GLenum blend_dstf;
    bool cull;
    GLenum cull_face;
    bool depth_test;
    bool depth_write;
    GLenum depth_func;
    FVec4 const_diffuse;
    float const_metallic;
    float const_roughness;
    Texture* tex_diffuse;
    Texture* tex_occ_met_rgh;
    Sampler smp_diffuse;
    Sampler smp_occ_met_rgh;
} Material;