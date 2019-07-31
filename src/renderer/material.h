#pragma once
#include <common.h>
#include <flib/accessor.h>
#include <flib/vec.h>

typedef enum {
    MATERIAL_UNLIT,
    MATERIAL_METALLIC_ROUGHNESS,
} MaterialType;

typedef struct {
    MaterialType type;
    bool blend;
    GLenum blend_srcf;
    GLenum blend_dstf;
    bool cull;
    GLenum cull_face;
    bool depth_test;
    bool depth_write;
    GLenum depth_func;
    FVec4 const_diffuse;
    FVec4 const_emissive;
    float const_metallic;
    float const_roughness;
} Material;