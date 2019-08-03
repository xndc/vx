#include "model.h"
#include <stb/stb_sprintf.h>
#include <parson/parson.h>
#include <glad/glad.h>
#include "texture.h"

#define X(name, dir, file) Model name = {0};
XM_ASSETS_MODELS_GLTF
#undef X

typedef struct {
    vec4 rotation;
    vec4 worldRotation;
    vec3 position;
    vec3 scale;
    vec3 worldPosition;
    vec3 worldScale;
    int parent; // -1 if this is a root node
    int meshId; // -1 if this node is not a mesh
    bool worldTransformComputed;
} Node;

static inline JSON_Value* ReadJSONFromFile (const char* filename);
static void ComputeWorldTransformForNode (Node* list, size_t index);
static void ReadModelFromDisk (const char* name, Model* model, const char* dir, const char* file);

void InitModelSystem() {
    #define X(name, dir, file) ReadModelFromDisk(#name, &name, dir, file);
    XM_ASSETS_MODELS_GLTF
    #undef X
}

static inline JSON_Value* ReadJSONFromFile (const char* filename) {
    JSON_Value* root = json_parse_file_with_comments(filename);
    if (root == NULL) {
        FILE* fp = fopen(filename, "r");
        if (fp == NULL) {
            VXPANIC("Failed to read file %s (%s)", filename, strerror(errno));
        } else {
            VXPANIC("Failed to parse JSON from %s (unknown error in parson)", filename);
        }
    }
    return root;
}

static void ReadModelFromDisk (const char* name, Model* model, const char* dir, const char* file) {
    // Read GLTF file:
    static char path [4096]; // 4095 characters really ought to be enough for anyone...
    stbsp_snprintf(path, VXSIZE(path), "%s/%s", dir, file);
    VXINFO("Loading GLTF model %s from %s...", name, path);
    JSON_Object* root = json_value_get_object(ReadJSONFromFile(path));

    // Extract texture information and load referenced textures into memory:
    JSON_Array* textures = json_object_get_array(root, "textures");
    JSON_Array* images   = json_object_get_array(root, "images");
    if (textures && images) {
        for (size_t i = 0; i < json_array_get_count(textures); i++) {
            JSON_Object* texture = json_array_get_object(textures, i);
            JSON_Value* imageIndexV = json_object_get_value(texture, "source");
            if (imageIndexV && json_value_get_type(imageIndexV) == JSONNumber) {
                int imageIndex = (int) json_value_get_number(imageIndexV);
                JSON_Object* image = json_array_get_object(images, imageIndex);
                const char* uri = json_object_get_string(image, "uri");
                if (uri) {
                    // TODO: should we spend the time to make this work with relative filenames?
                    stbsp_snprintf(path, VXSIZE(path), "%s/%s", dir, uri);
                    GLuint gltexture = LoadTextureFromDisk(uri, path);
                    arrput(model->textures, gltexture);
                }
            }
        }
    }

    // Extract buffer information and load referenced buffer files into memory:
    JSON_Array* buffers = json_object_get_array(root, "buffers");
    size_t bufcount = 0;
    if (buffers) {
        bufcount = json_array_get_count(buffers);
        for (size_t i = 0; i < bufcount; i++) {
            JSON_Object* buffer = json_array_get_object(buffers, i);
            const char* uri = json_object_get_string(buffer, "uri");
            size_t len = (size_t) json_object_get_number(buffer, "byteLength");
            if (uri && len != 0) {
                stbsp_snprintf(path, 4096, "%s/%s", dir, uri);
                char* buf = vxReadFile(path, false, NULL); // TODO: check length?
                arrput(model->buffers, buf);
                arrput(model->bufsizes, len);
            }
        }
    }

    // Extract accessor data into a temporary buffer for now:
    // FIXME: support for max/min properties on accessors?
    JSON_Array* gltf_accessors   = json_object_get_array(root, "accessors");
    JSON_Array* gltf_bufferviews = json_object_get_array(root, "bufferViews");
    FAccessor* accessors = NULL;
    if (gltf_accessors && gltf_bufferviews) {
        for (size_t i = 0; i < json_array_get_count(gltf_accessors); i++) {
            JSON_Object* gltf_accessor = json_array_get_object(gltf_accessors, i);
            if (json_object_has_value(gltf_accessor, "sparse")) {
                VXPANIC("Sparse accessors are not supported yet");
            }
            // Relevant data from GLTF accessor:
            const char* type = json_object_get_string(gltf_accessor, "type");
            uint32_t componentType = (uint32_t) json_object_get_number(gltf_accessor, "componentType");
            size_t bufferViewId = (size_t) json_object_get_number(gltf_accessor, "bufferView");
            size_t accBufferOffset = (size_t) json_object_get_number(gltf_accessor, "byteOffset");
            size_t accByteStride = (size_t) json_object_get_number(gltf_accessor, "byteStride");
            size_t accElementCount = (size_t) json_object_get_number(gltf_accessor, "count");
            // Relevant data from corresponding GLTF bufferview:
            JSON_Object* bufferView = json_array_get_object(gltf_bufferviews, bufferViewId);
            size_t bufferIndex = (size_t) json_object_get_number(bufferView, "buffer");
            size_t bufferOffset = (size_t) json_object_get_number(bufferView, "byteOffset");
            // Create accessor:
            FAccessor acc;
            FAccessorInit(&acc, FAccessorTypeFromGltf(type, componentType),
                model->buffers[bufferIndex], bufferOffset + accBufferOffset, accElementCount,
                (uint8_t) accByteStride /* which is 0 if the key is missing */);
            arrput(accessors, acc);
        }
    }

    // Extract material data:
    JSON_Array* materials = json_object_get_array(root, "materials");
    if (materials) {
        for (size_t i = 0; i < json_array_get_count(materials); i++) {
            JSON_Object* material = json_array_get_object(materials, i);
            Material mat;
            JSON_Object* pbrMR = json_object_get_object(material, "pbrMetallicRoughness");
            if (pbrMR) {
                if (json_object_has_value(pbrMR, "baseColorTexture")) {
                    size_t colorTexId = (size_t) json_object_dotget_number(pbrMR, "baseColorTexture.index");
                    mat.tex_diffuse = model->textures[colorTexId];
                    glGenSamplers(1, &mat.smp_diffuse);
                    glSamplerParameteri(mat.smp_diffuse, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                    glSamplerParameteri(mat.smp_diffuse, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glSamplerParameteri(mat.smp_diffuse, GL_TEXTURE_WRAP_S, GL_REPEAT);
                    glSamplerParameteri(mat.smp_diffuse, GL_TEXTURE_WRAP_T, GL_REPEAT);
                }
                if (json_object_has_value(pbrMR, "metallicRoughnessTexture")) {
                    size_t mrTexId = (size_t) json_object_dotget_number(pbrMR, "metallicRoughnessTexture.index");
                    mat.tex_occ_met_rgh = model->textures[mrTexId];
                    glGenSamplers(1, &mat.smp_occ_met_rgh);
                    glSamplerParameteri(mat.smp_occ_met_rgh, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                    glSamplerParameteri(mat.smp_occ_met_rgh, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glSamplerParameteri(mat.smp_occ_met_rgh, GL_TEXTURE_WRAP_S, GL_REPEAT);
                    glSamplerParameteri(mat.smp_occ_met_rgh, GL_TEXTURE_WRAP_T, GL_REPEAT);
                }
            }
            if (json_object_has_value(material, "normalTexture")) {
                size_t normalTexId = (size_t) json_object_dotget_number(material, "normalTexture.index");
                mat.tex_normal = model->textures[normalTexId];
                glGenSamplers(1, &mat.smp_normal);
                glSamplerParameteri(mat.smp_normal, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glSamplerParameteri(mat.smp_normal, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glSamplerParameteri(mat.smp_normal, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glSamplerParameteri(mat.smp_normal, GL_TEXTURE_WRAP_T, GL_REPEAT);
            }
            // TODO: constant colors, emissive, other properties?
        }
    }

    // Extract meshes and transforms from the GLTF node-mesh-primitive structure:
    JSON_Array* nodes  = json_object_get_array(root, "nodes");
    JSON_Array* meshes = json_object_get_array(root, "meshes");
    if (nodes && meshes) {
        // Allocate and clear out Node list:
        size_t nodeCount = json_array_get_count(nodes);
        Node* nodeEntries = VXGENALLOC(nodeCount, Node);
        for (size_t i = 0; i < nodeCount; i++) {
            Node* nentry = &nodeEntries[i];
            nentry->parent = -1;
            nentry->meshId = -1;
        }
        // First pass: store parents and local transform data, and compute primitive count
        size_t totalPrimitives = 0;
        for (size_t i = 0; i < nodeCount; i++) {
            Node* nentry = &nodeEntries[i];
            JSON_Object* node = json_array_get_object(nodes, i);
            if (json_object_has_value_of_type(node, "children", JSONArray)) {
                JSON_Array* children = json_object_get_array(node, "children");
                for (size_t ich = 0; ich < json_array_get_count(children); ich++) {
                    size_t childNodeIdx = (size_t) json_array_get_number(children, ich);
                    nodeEntries[childNodeIdx].parent = (int) i;
                }
            }
            // Translation:
            if (json_object_has_value_of_type(node, "translation", JSONArray)) {
                nentry->position[0] = (float) json_array_get_number(json_object_get_array(node, "translation"), 0);
                nentry->position[1] = (float) json_array_get_number(json_object_get_array(node, "translation"), 1);
                nentry->position[2] = (float) json_array_get_number(json_object_get_array(node, "translation"), 2);
            }
            // Scale:
            if (json_object_has_value_of_type(node, "scale", JSONArray)) {
                nentry->scale[0] = (float) json_array_get_number(json_object_get_array(node, "scale"), 0);
                nentry->scale[1] = (float) json_array_get_number(json_object_get_array(node, "scale"), 1);
                nentry->scale[2] = (float) json_array_get_number(json_object_get_array(node, "scale"), 2);
            } else {
                glm_vec3_one(nentry->scale);
            }
            // Rotation:
            if (json_object_has_value_of_type(node, "rotation", JSONArray)) {
                nentry->rotation[0] = (float) json_array_get_number(json_object_get_array(node, "rotation"), 0);
                nentry->rotation[1] = (float) json_array_get_number(json_object_get_array(node, "rotation"), 1);
                nentry->rotation[2] = (float) json_array_get_number(json_object_get_array(node, "rotation"), 2);
                nentry->rotation[3] = (float) json_array_get_number(json_object_get_array(node, "rotation"), 3);
            } else {
                glm_quat_identity(nentry->rotation);
            }
            // Compute primitive count:
            if (json_object_has_value_of_type(node, "mesh", JSONNumber)) {
                nentry->meshId = (int) json_object_get_number(node, "mesh");
                JSON_Object* mesh = json_array_get_object(meshes, nentry->meshId);
                JSON_Array* primitives = json_object_get_array(mesh, "primitives");
                totalPrimitives += json_array_get_count(primitives);
            }
        }
        // Second pass: recursively compute world transform data
        for (size_t i = 0; i < nodeCount; i++) {
            ComputeWorldTransformForNode(nodeEntries, i);
        }
        // Third pass: store meshes (i.e. GLTF primitives, not GLTF meshes) for each node
        arrsetlen(model->transforms, totalPrimitives);
        arrsetlen(model->meshes,     totalPrimitives);
        size_t meshidx = 0;
        for (size_t i = 0; i < nodeCount; i++) {
            Node* nentry = &nodeEntries[i];
            JSON_Object* node = json_array_get_object(nodes, i);
            if (json_object_has_value_of_type(node, "mesh", JSONNumber)) {
                nentry->meshId = (int) json_object_get_number(node, "mesh");
                JSON_Object* mesh = json_array_get_object(meshes, nentry->meshId);
                JSON_Array* primitives = json_object_get_array(mesh, "primitives");
                for (size_t pidx = 0; pidx < json_array_get_count(primitives); pidx++) {
                    JSON_Object* primitive = json_array_get_object(primitives, pidx);
                    // Transform:
                    glm_mat4_identity(model->transforms[meshidx]);
                    glm_translate(model->transforms[meshidx], nentry->worldPosition);
                    glm_quat_rotate(model->transforms[meshidx], nentry->worldRotation, model->transforms[meshidx]);
                    glm_scale(model->transforms[meshidx], nentry->worldScale);
                    // Indices:
                    size_t accessorId = (size_t) json_object_get_number(primitive, "indices");
                    model->meshes[i].indices = accessors[accessorId];
                    // Attributes:
                    JSON_Object* attributes = json_object_get_object(primitive, "attributes");
                    #define X(gltfName, dest) \
                        if (json_object_has_value_of_type(attributes, gltfName, JSONNumber)) { \
                            accessorId = (size_t) json_object_get_number(attributes, gltfName); \
                            dest = accessors[accessorId]; \
                        }
                    X("POSITION",   model->meshes[i].positions);
                    X("NORMAL",     model->meshes[i].normals);
                    X("TANGENT",    model->meshes[i].tangents);
                    X("TEXCOORD_0", model->meshes[i].texcoords0);
                    X("TEXCOORD_1", model->meshes[i].texcoords1);
                    X("COLOR_0",    model->meshes[i].colors);
                    X("JOINTS_0",   model->meshes[i].joints);
                    X("WEIGHTS_0",  model->meshes[i].weights)
                    #undef X
                    // Material:
                    size_t materialId = (size_t) json_object_get_number(primitive, "material");
                    model->meshes[i].material = &model->materials[materialId];
                    // Debug output:
                    VXINFO("Stored mesh %d (from node %d, mesh %d, primitive %d)",
                        meshidx, i, nentry->meshId, pidx);
                    meshidx++;
                }
            }
        }
    }
    VXINFO("Successfully loaded GLTF model %s", name);
    // FIXME: memory leaks: JSON tree, accessors
}

static void ComputeWorldTransformForNode (Node* list, size_t index) {
    Node* node = &list[index];
    if (!node->worldTransformComputed) {
        if (node->parent == -1) {
            glm_vec3_copy(node->position, node->worldPosition);
            glm_quat_copy(node->rotation, node->worldRotation);
            glm_vec3_copy(node->scale,    node->worldScale);
        } else {
            ComputeWorldTransformForNode(list, node->parent);
            Node* parent = &list[node->parent];
            glm_vec3_add(node->position, parent->worldPosition, node->worldPosition);
            glm_quat_mul(node->rotation, parent->worldRotation, node->worldRotation); // FIXME: correct order?
            glm_vec3_mul(node->scale,    parent->worldScale,    node->worldScale);
        }
        node->worldTransformComputed = true;
    }
}