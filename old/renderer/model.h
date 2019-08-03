#pragma once
#include <common.h>
#include <flib/vec.h>
#include "mesh.h"

typedef struct {
    size_t count;
    Mesh* meshes;
    FMat4 transforms;
} Model;