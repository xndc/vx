#pragma once
#include "common.h"
#include "assets.h"
#include "flib/accessor.h"

typedef struct {
    bool blend;
    GLenum blend_srcf;
    GLenum blend_dstf;
    bool cull;
    GLenum cull_face;
    bool depth_test;
    bool depth_write;
    GLenum depth_func;
    vec4 const_diffuse;
    float const_metallic;
    float const_roughness;
    GLuint tex_diffuse;
    GLuint smp_diffuse;
    GLuint tex_occ_met_rgh;
    GLuint smp_occ_met_rgh;
    GLuint tex_occlusion;
    GLuint smp_occlusion;
    GLuint tex_metallic;
    GLuint smp_metallic;
    GLuint tex_roughness;
    GLuint smp_roughness;
    GLuint tex_normal;
    GLuint smp_normal;
} Material;

void InitMaterial (Material* m);

typedef struct {
    GLuint gl_vertex_array;
    FAccessor positions;
    FAccessor normals;
    FAccessor tangents;
    FAccessor texcoords0;
    FAccessor texcoords1;
    FAccessor colors;
    FAccessor joints;
    FAccessor weights;
    FAccessor indices;
    Material* material;
} Mesh;

typedef struct {
    char** buffers;
    size_t* bufsizes;
    GLuint* textures;
    Material* materials;
    Mesh* meshes;
    mat4* transforms;
} Model;

#define X(name, dir, file) extern Model name;
XM_ASSETS_MODELS_GLTF
#undef X

// Reads a model from disk.
void ReadModelFromDisk (const char* name, Model* model, const char* dir, const char* file);

// Uploads or reuploads a mesh to the GPU.
void UploadMeshToGPU (Mesh* mesh);