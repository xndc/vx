#pragma once
#include <common.h>
#include <flib/accessor.h>
#include "material.h"

typedef struct {
    GLuint gl_vao;
    bool gl_dynamic;
    FAccessor faces;
    FAccessor positions;
    FAccessor normals;
    FAccessor tangents;
    FAccessor texcoords0;
    FAccessor texcoords1;
    Material* material;
} Mesh;