#include "model.h"
#include <stb/stb_sprintf.h>
#include <parson/parson.h>
#include <glad/glad.h>
#include "texture.h"
#include "render/render.h"

#define X(name, dir, file) Model name = {0};
XM_ASSETS_MODELS_GLTF
#undef X

#define NO_GLTF_LOADER_DEBUG

#ifndef NO_GLTF_LOADER_DEBUG
    #define LOADER_DEBUG(...) VXDEBUG(__VA_ARGS__)
#else
    #define LOADER_DEBUG(...)
#endif

typedef struct {
    mat4 matrix;
    mat4 worldMatrix;
    int parent; // -1 if this is a root node
    int meshId; // -1 if this node is not a mesh
    bool worldTransformComputed;
} Node;

static inline JSON_Value* ReadJSONFromFile (const char* filename);
static void ComputeWorldTransformForNode (Node* list, size_t index);

// void InitModelSystem() {
//     #define X(name, dir, file) ReadModelFromDisk(#name, &name, dir, file);
//     XM_ASSETS_MODELS_GLTF
//     #undef X
// }

void InitMaterial (Material* m) {
    memset(m, 0, sizeof(Material));
    m->blend = false;
    m->blend_srcf = GL_SRC_ALPHA;           // suitable for back-to-front transparency
    m->blend_dstf = GL_ONE_MINUS_SRC_ALPHA; // suitable for back-to-front transparency
    m->cull = true;
    m->cull_face = GL_BACK;
    m->depth_test = true;
    m->depth_write = true;
    m->depth_func = GL_GREATER;
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

void ReadModelFromDisk (const char* name, Model* model, const char* dir, const char* file) {
    LOADER_DEBUG("ReadModelFromFile(%s, 0x%jx, %s, %s)", name, model, dir, file);
    // Read GLTF file:
    static char path [4096]; // 4095 characters really ought to be enough for anyone...
    stbsp_snprintf(path, VXSIZE(path), "%s/%s", dir, file);
    VXINFO("Loading GLTF model %s from %s...", name, path);
    JSON_Object* root = json_value_get_object(ReadJSONFromFile(path));

    // Extract texture information and load referenced textures into memory:
    JSON_Array* textures = json_object_get_array(root, "textures");
    JSON_Array* images   = json_object_get_array(root, "images");
    if (textures && images) {
        size_t texcount = json_array_get_count(textures);
        for (size_t i = 0; i < texcount; i++) {
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
        LOADER_DEBUG("Loading %ju accessors:", json_array_get_count(gltf_accessors));
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
            FAccessor* p = &arrlast(accessors);
            LOADER_DEBUG("* idx %jd p 0x%jx buffer 0x%jx offset %ju count %ju type %s/%d (t %d cc %d cs %d st %d)",
                arrlen(accessors) - 1, p, p->buffer, p->offset, p->count, type, componentType,
                p->type, p->component_count, p->component_size, p->stride);
            LOADER_DEBUG("  from accessor %ju, bufferview %ju, buffer %ju", i, bufferViewId, bufferIndex);
        }
    }

    // Extract material data:
    JSON_Array* materials = json_object_get_array(root, "materials");
    if (materials) {
        LOADER_DEBUG("Loading %ju materials:", json_array_get_count(materials));
        for (size_t i = 0; i < json_array_get_count(materials); i++) {
            JSON_Object* material = json_array_get_object(materials, i);
            Material mat = {0};
            InitMaterial(&mat);
            JSON_Object* pbrMR = json_object_get_object(material, "pbrMetallicRoughness");
            if (pbrMR) {
                LOADER_DEBUG("* Material %ju, using PBR Metallic-Roughness workflow", i);
                if (json_object_has_value(pbrMR, "baseColorTexture")) {
                    size_t colorTexId = (size_t) json_object_dotget_number(pbrMR, "baseColorTexture.index");
                    mat.tex_diffuse = model->textures[colorTexId];
                    glGenSamplers(1, &mat.smp_diffuse);
                    glSamplerParameteri(mat.smp_diffuse, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                    glSamplerParameteri(mat.smp_diffuse, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glSamplerParameteri(mat.smp_diffuse, GL_TEXTURE_WRAP_S, GL_REPEAT);
                    glSamplerParameteri(mat.smp_diffuse, GL_TEXTURE_WRAP_T, GL_REPEAT);
                    LOADER_DEBUG("  -> diffuse color texture idx %ju gltex %ju glsmp %ju", colorTexId,
                        mat.tex_diffuse, mat.smp_diffuse);
                }
                if (json_object_has_value(pbrMR, "metallicRoughnessTexture")) {
                    size_t mrTexId = (size_t) json_object_dotget_number(pbrMR, "metallicRoughnessTexture.index");
                    mat.tex_occ_met_rgh = model->textures[mrTexId];
                    glGenSamplers(1, &mat.smp_occ_met_rgh);
                    glSamplerParameteri(mat.smp_occ_met_rgh, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                    glSamplerParameteri(mat.smp_occ_met_rgh, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glSamplerParameteri(mat.smp_occ_met_rgh, GL_TEXTURE_WRAP_S, GL_REPEAT);
                    glSamplerParameteri(mat.smp_occ_met_rgh, GL_TEXTURE_WRAP_T, GL_REPEAT);
                    LOADER_DEBUG("  -> occ-met-rgh texture idx %ju gltex %ju glsmp %ju", mrTexId,
                        mat.tex_occ_met_rgh, mat.smp_occ_met_rgh);
                }
            } else {
                LOADER_DEBUG("* Material %ju, using an unknown workflow", i);
            }
            if (json_object_has_value(material, "normalTexture")) {
                size_t normalTexId = (size_t) json_object_dotget_number(material, "normalTexture.index");
                mat.tex_normal = model->textures[normalTexId];
                glGenSamplers(1, &mat.smp_normal);
                glSamplerParameteri(mat.smp_normal, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glSamplerParameteri(mat.smp_normal, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glSamplerParameteri(mat.smp_normal, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glSamplerParameteri(mat.smp_normal, GL_TEXTURE_WRAP_T, GL_REPEAT);
                LOADER_DEBUG("  -> normal map texture idx %ju gltex %ju glsmp %ju", normalTexId,
                    mat.tex_normal, mat.smp_normal);
            }
            // TODO: constant colors, emissive, other properties?
            arrput(model->materials, mat);
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
            glm_mat4_zero(nentry->matrix);
            glm_mat4_zero(nentry->worldMatrix);
            nentry->worldTransformComputed = false;
        }
        LOADER_DEBUG("Loading %ju nodes into Node array 0x%jx:", nodeCount, nodeEntries);
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
            if (json_object_has_value_of_type(node, "matrix", JSONArray)) {
                // Already-specified matrix:
                JSON_Array* a = json_object_get_array(node, "matrix");
                for (size_t aidx = 0; aidx < 16; aidx++) {
                    ((float*)nentry->matrix)[aidx] = json_array_get_number(a, aidx);
                }
            } else {
                // Translation, scale and rotation:
                vec4 rotation = GLM_QUAT_IDENTITY_INIT;
                vec3 translation = GLM_VEC3_ZERO_INIT;
                vec3 scale = GLM_VEC3_ONE_INIT;
                if (json_object_has_value_of_type(node, "rotation", JSONArray)) {
                    JSON_Array* a = json_object_get_array(node, "rotation");
                    for (size_t aidx = 0; aidx < 4; aidx++) {
                        rotation[aidx] = json_array_get_number(a, aidx);
                    }
                }
                if (json_object_has_value_of_type(node, "translation", JSONArray)) {
                    JSON_Array* a = json_object_get_array(node, "translation");
                    for (size_t aidx = 0; aidx < 3; aidx++) {
                        translation[aidx] = json_array_get_number(a, aidx);
                    }
                }
                if (json_object_has_value_of_type(node, "scale", JSONArray)) {
                    JSON_Array* a = json_object_get_array(node, "scale");
                    for (size_t aidx = 0; aidx < 3; aidx++) {
                        scale[aidx] = json_array_get_number(a, aidx);
                    }
                }
                glm_mat4_identity(nentry->matrix);
                glm_translate(nentry->matrix, translation);
                glm_quat_rotate(nentry->matrix, rotation, nentry->matrix);
                glm_scale(nentry->matrix, scale);
            }
            // Compute primitive count:
            if (json_object_has_value_of_type(node, "mesh", JSONNumber)) {
                nentry->meshId = (int) json_object_get_number(node, "mesh");
                JSON_Object* mesh = json_array_get_object(meshes, nentry->meshId);
                JSON_Array* primitives = json_object_get_array(mesh, "primitives");
                totalPrimitives += json_array_get_count(primitives);
                LOADER_DEBUG("* Node %ju with attached mesh %ju (0x%jx) containing %ju primitives (%ju total so far):",
                    i, nentry->meshId, mesh, json_array_get_count(primitives), totalPrimitives);
            } else {
                LOADER_DEBUG("* Node %ju with no primitives attached:", i);
            }
            float* m = nentry->matrix;
            LOADER_DEBUG("  * [[%.04f %.04f %.04f %.04f] [%.04f %.04f %.04f %.04f] "
                "[%.04f %.04f %.04f %.04f] [%.04f %.04f %.04f %.04f]]",
                m[0],  m[1],  m[2],  m[3],
                m[4],  m[5],  m[6],  m[7],
                m[8],  m[9],  m[10], m[11],
                m[12], m[13], m[14], m[15]);
        }
        // Second pass: recursively compute world transform data
        LOADER_DEBUG("Computing world transforms for nodes:");
        for (size_t i = 0; i < nodeCount; i++) {
            ComputeWorldTransformForNode(nodeEntries, i);
            Node* nentry = &nodeEntries[i];
            float* m = nentry->worldMatrix;
            LOADER_DEBUG("* Node %ju: [[%.04f %.04f %.04f %.04f] [%.04f %.04f %.04f %.04f] "
                "[%.04f %.04f %.04f %.04f] [%.04f %.04f %.04f %.04f]]", i,
                m[0],  m[1],  m[2],  m[3],
                m[4],  m[5],  m[6],  m[7],
                m[8],  m[9],  m[10], m[11],
                m[12], m[13], m[14], m[15]);
        }
        // Third pass: store meshes (i.e. GLTF primitives, not GLTF meshes) for each node
        LOADER_DEBUG("Extracting meshes from each node:");
        arrsetlen(model->transforms, totalPrimitives);
        arrsetlen(model->meshes,     totalPrimitives);
        size_t meshidx = 0;
        for (size_t i = 0; i < nodeCount; i++) {
            Node* nentry = &nodeEntries[i];
            JSON_Object* node = json_array_get_object(nodes, i);
            if (json_object_has_value_of_type(node, "mesh", JSONNumber)) {
                nentry->meshId = (int) json_object_get_number(node, "mesh");
                LOADER_DEBUG("* Node %ju with mesh idx %d:", i, nentry->meshId);
                JSON_Object* mesh = json_array_get_object(meshes, nentry->meshId);
                JSON_Array* primitives = json_object_get_array(mesh, "primitives");
                for (size_t pidx = 0; pidx < json_array_get_count(primitives); pidx++) {
                    JSON_Object* primitive = json_array_get_object(primitives, pidx);
                    LOADER_DEBUG("  * Primitive %ju => mesh %ju:", pidx, meshidx);
                    // Clear out:
                    memset(&model->meshes[meshidx], 0, sizeof(model->meshes[meshidx]));
                    // Transform:
                    float* m = model->transforms[meshidx];
                    glm_mat4_copy(nentry->worldMatrix, m);
                    LOADER_DEBUG("    * [[%.04f %.04f %.04f %.04f] [%.04f %.04f %.04f %.04f] "
                        "[%.04f %.04f %.04f %.04f] [%.04f %.04f %.04f %.04f]]",
                        m[0],  m[1],  m[2],  m[3],
                        m[4],  m[5],  m[6],  m[7],
                        m[8],  m[9],  m[10], m[11],
                        m[12], m[13], m[14], m[15]);
                    // Indices:
                    size_t accessorId = (size_t) json_object_get_number(primitive, "indices");
                    model->meshes[meshidx].indices = accessors[accessorId];
                    LOADER_DEBUG("    * Indices: accessor idx %ju, p 0x%jx, buf 0x%jx => mesh acc 0x%jx buf 0x%jx",
                        accessorId, &accessors[accessorId], accessors[accessorId].buffer,
                        &model->meshes[meshidx].indices, model->meshes[meshidx].indices.buffer);
                    // Attributes:
                    JSON_Object* attributes = json_object_get_object(primitive, "attributes");
                    #define X(gltfName, dest) \
                        if (json_object_has_value_of_type(attributes, gltfName, JSONNumber)) { \
                            accessorId = (size_t) json_object_get_number(attributes, gltfName); \
                            dest = accessors[accessorId]; \
                            LOADER_DEBUG("    * Attribute %s: accessor idx %ju, p 0x%jx, buf 0x%jx => mesh acc 0x%jx buf 0x%jx", \
                                gltfName, accessorId, &accessors[accessorId], accessors[accessorId].buffer, \
                                &dest, dest.buffer); \
                        }
                    X("POSITION",   model->meshes[meshidx].positions);
                    X("NORMAL",     model->meshes[meshidx].normals);
                    X("TANGENT",    model->meshes[meshidx].tangents);
                    X("TEXCOORD_0", model->meshes[meshidx].texcoords0);
                    X("TEXCOORD_1", model->meshes[meshidx].texcoords1);
                    X("COLOR_0",    model->meshes[meshidx].colors);
                    X("JOINTS_0",   model->meshes[meshidx].joints);
                    X("WEIGHTS_0",  model->meshes[meshidx].weights)
                    #undef X
                    // Material:
                    size_t materialId = (size_t) json_object_get_number(primitive, "material");
                    model->meshes[meshidx].material = &model->materials[materialId];
                    LOADER_DEBUG("    * Material idx %ju (0x%jx)", materialId, model->meshes[meshidx].material);
                    // Done:
                    meshidx++;
                }
            }
        }
    }

    LOADER_DEBUG("Uploading VBOs for %ju meshes:", arrlenu(model->meshes));
    for (size_t i = 0; i < arrlenu(model->meshes); i++) {
        Mesh* m = &model->meshes[i];
        glGenVertexArrays(1, &m->gl_vertex_array);
        glBindVertexArray(m->gl_vertex_array);
        LOADER_DEBUG("* Mesh %ju => VAO %u", i, m->gl_vertex_array);

        #define XMLOCAL_EBO \
            X(m->indices)
        #define XMLOCAL_VBO \
            X(ATTR_POSITION,   m->positions) \
            X(ATTR_NORMAL,     m->normals) \
            X(ATTR_TANGENT,    m->tangents) \
            X(ATTR_TEXCOORD0,  m->texcoords0) \
            X(ATTR_TEXCOORD1,  m->texcoords1) \
            X(ATTR_COLOR,      m->colors) \
            X(ATTR_JOINTS,     m->joints) \
            X(ATTR_WEIGHTS,    m->weights)

        #define X(acc) \
            if (acc.buffer != NULL) { \
                glGenBuffers(1, &acc.gl_object); \
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, acc.gl_object); \
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizei) (acc.count * (size_t) acc.stride), \
                    FAccessorData(&acc), GL_STATIC_DRAW); \
                LOADER_DEBUG("  * " #acc ": EBO %u", acc.gl_object); \
            }
        XMLOCAL_EBO
        #undef X

        #define X(attr, acc) \
            if (acc.buffer != NULL) { \
                glGenBuffers(1, &acc.gl_object); \
                glBindBuffer(GL_ARRAY_BUFFER, acc.gl_object); \
                glBufferData(GL_ARRAY_BUFFER, (GLsizei) (acc.count * (size_t) acc.stride), \
                    FAccessorData(&acc), GL_STATIC_DRAW); \
                glBindBuffer(GL_ARRAY_BUFFER, acc.gl_object); \
                glEnableVertexAttribArray(attr); \
                glVertexAttribPointer(attr, acc.component_count, GL_FLOAT, false, acc.stride, NULL); \
                LOADER_DEBUG("  * " #acc ": VBO %u", acc.gl_object); \
            }
        XMLOCAL_VBO
        #undef X

        #undef XMLOCAL_EBO
        #undef XMLOCAL_VBO
    }

    VXINFO("Loaded GLTF model %s from %s/%s", name, dir, file);
    // FIXME: memory leaks: JSON tree, accessors
}

static void ComputeWorldTransformForNode (Node* list, size_t index) {
    Node* node = &list[index];
    if (!node->worldTransformComputed) {
        if (node->parent == -1) {
            glm_mat4_copy(node->matrix, node->worldMatrix);
        } else {
            ComputeWorldTransformForNode(list, node->parent);
            Node* parent = &list[node->parent];
            // FIXME: is this the correct order?
            glm_mat4_mul(node->matrix, parent->worldMatrix, node->worldMatrix);
        }
        node->worldTransformComputed = true;
    }
}