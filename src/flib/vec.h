#pragma once
#include <common.h>

typedef struct {
    union { float x; float r; float s; float u; };
    union { float y; float g; float t; float v; };
} FVec2;

typedef struct {
    union { float x; float r; float s; };
    union { float y; float g; float t; };
    union { float z; float b; float u; };
} FVec3;

typedef struct {
    union { float x; float r; float s; };
    union { float y; float g; float t; };
    union { float z; float b; float u; };
    union { float w; float a; float v; };
} FVec4;