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

typedef struct {
    union {
        struct {
            float a11, a12, a13, a14;
            float a21, a22, a23, a24;
            float a31, a32, a33, a34;
            float a41, a42, a43, a44;
        };
        FVec4 v[4];
        float f[16];
    };
} FMat4;