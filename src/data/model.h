#pragma once
#include "common.h"
#include "assets.h"
#include "flib/accessor.h"

typedef struct Material {
    bool blend;
    GLenum blend_srcf;
    GLenum blend_dstf;
    bool stipple;
    float stipple_soft_cutoff;
    float stipple_hard_cutoff;
    bool cull;
    GLenum cull_face;
    bool depth_test;
    bool depth_write;
    GLenum depth_func;
    vec4 const_diffuse;
    float const_occlusion;
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

typedef struct Mesh {
    GLenum type; // GL_TRIANGLES, etc.
    GLuint gl_vertex_array;
    GLuint gl_element_array;
    size_t gl_element_count;
    size_t gl_element_type;
    size_t gl_vertex_count;
    #if 0
    FAccessor positions;
    FAccessor normals;
    FAccessor tangents;
    FAccessor texcoords0;
    FAccessor texcoords1;
    FAccessor colors;
    FAccessor joints;
    FAccessor weights;
    FAccessor indices;
    #endif
    Material* material;
} Mesh;

typedef struct Model {
    size_t textureCount;
    GLuint* textures;
    size_t materialCount;
    Material* materials;
    size_t meshCount;
    mat4* meshTransforms;
    Mesh* meshes;
} Model;

#define X(name, dir, file) extern Model name;
XM_ASSETS_MODELS_GLTF
#undef X

// Reads a model from disk.
void ReadModelFromDisk (const char* name, Model* model, const char* dir, const char* file);

// Uploads or reuploads a mesh to the GPU.
void UploadMeshToGPU (Mesh* mesh);