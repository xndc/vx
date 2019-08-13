#include "model.h"
#include <stb_sprintf.h>
#include <parson/parson.h>
#include <glad/glad.h>
#include "texture.h"
#include "render/render.h"

#define X(name, dir, file) Model name = {0};
XM_ASSETS_MODELS_GLTF
#undef X

#if 0
#define NO_GLTF_LOADER_DEBUG
#ifndef NO_GLTF_LOADER_DEBUG
    #define LOADER_DEBUG(...) vxDebug(__VA_ARGS__)
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
#endif

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
    m->stipple = false;
    // NOTE: 0.5 is the GLTF default alpha cutoff. GLTF also doesn't include the concept of a soft
    //   cutoff at all, so setting it to anything over 0.5 would result in models rendering
    //   incorrectly. This is very visible on Sponza, for instance.
    m->stipple_hard_cutoff = 0.5f; // below this alpha value, the pixel is not rendered at all
    m->stipple_soft_cutoff = 0.5f; // above this alpha value, the pixel is always rendered
    m->cull = true;
    m->cull_face = GL_BACK;
    m->depth_test = true;
    m->depth_write = true;
    m->depth_func = GL_GREATER;
    glm_vec4_one(m->const_diffuse);
    m->const_metallic  = 1.0f;
    m->const_roughness = 1.0f;
}

#if 0
static inline JSON_Value* ReadJSONFromFile (const char* filename) {
    JSON_Value* root = json_parse_file_with_comments(filename);
    if (root == NULL) {
        FILE* fp = fopen(filename, "r");
        if (fp == NULL) {
            vxPanic("Failed to read file %s (%s)", filename, strerror(errno));
        } else {
            vxPanic("Failed to parse JSON from %s (unknown error in parson)", filename);
        }
    }
    return root;
}

void ReadModelFromDisk (const char* name, Model* model, const char* dir, const char* file) {
    LOADER_DEBUG("ReadModelFromFile(%s, 0x%jx, %s, %s)", name, model, dir, file);
    // Read GLTF file:
    static char path [4096]; // 4095 characters really ought to be enough for anyone...
    stbsp_snprintf(path, vxSize(path), "%s/%s", dir, file);
    vxLog("Loading GLTF model %s from %s...", name, path);
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
                    stbsp_snprintf(path, vxSize(path), "%s/%s", dir, uri);
                    GLuint gltexture = LoadTextureFromDisk(uri, path);
                    stbds_arrput(model->textures, gltexture);
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
                char* buf = vxReadFile(path, "rb", NULL); // TODO: check length?
                stbds_arrput(model->buffers, buf);
                stbds_arrput(model->bufsizes, len);
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
                vxPanic("Sparse accessors are not supported yet");
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
            stbds_arrput(accessors, acc);
            FAccessor* p = &stbds_arrlast(accessors);
            LOADER_DEBUG("* idx %jd p 0x%jx buffer 0x%jx offset %ju count %ju type %s/%d (t %d cc %d cs %d st %d)",
                stbds_arrlen(accessors) - 1, p, p->buffer, p->offset, p->count, type, componentType,
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
            stbds_arrput(model->materials, mat);
        }
    }

    // Extract meshes and transforms from the GLTF node-mesh-primitive structure:
    JSON_Array* nodes  = json_object_get_array(root, "nodes");
    JSON_Array* meshes = json_object_get_array(root, "meshes");
    if (nodes && meshes) {
        // Allocate and clear out Node list:
        size_t nodeCount = json_array_get_count(nodes);
        Node* nodeEntries = vxAlloc(nodeCount, Node);
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
                    ((float*)nentry->matrix)[aidx] = (float) json_array_get_number(a, aidx);
                }
            } else {
                // Translation, scale and rotation:
                vec4 rotation = GLM_QUAT_IDENTITY_INIT;
                vec3 translation = GLM_VEC3_ZERO_INIT;
                vec3 scale = GLM_VEC3_ONE_INIT;
                if (json_object_has_value_of_type(node, "rotation", JSONArray)) {
                    JSON_Array* a = json_object_get_array(node, "rotation");
                    for (size_t aidx = 0; aidx < 4; aidx++) {
                        rotation[aidx] = (float) json_array_get_number(a, aidx);
                    }
                }
                if (json_object_has_value_of_type(node, "translation", JSONArray)) {
                    JSON_Array* a = json_object_get_array(node, "translation");
                    for (size_t aidx = 0; aidx < 3; aidx++) {
                        translation[aidx] = (float) json_array_get_number(a, aidx);
                    }
                }
                if (json_object_has_value_of_type(node, "scale", JSONArray)) {
                    JSON_Array* a = json_object_get_array(node, "scale");
                    for (size_t aidx = 0; aidx < 3; aidx++) {
                        scale[aidx] = (float) json_array_get_number(a, aidx);
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
        stbds_arrsetlen(model->transforms, totalPrimitives);
        stbds_arrsetlen(model->meshes,     totalPrimitives);
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

    for (size_t i = 0; i < stbds_arrlenu(model->meshes); i++) {
        UploadMeshToGPU(&model->meshes[i]);
    }

    vxLog("Loaded GLTF model %s from %s/%s", name, dir, file);
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

// TODO: support for reuploading
void UploadMeshToGPU (Mesh* m) {
    glGenVertexArrays(1, &m->gl_vertex_array);
    glBindVertexArray(m->gl_vertex_array);
    vxDebug("Uploading mesh 0x%jx to GPU as VAO %u", m, m->gl_vertex_array);

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
            LOADER_DEBUG("* " #acc ": EBO %u", acc.gl_object); \
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
            LOADER_DEBUG("* " #acc ": VBO %u %s %d", acc.gl_object, #attr, attr); \
        }
    XMLOCAL_VBO
    #undef X

    #undef XMLOCAL_EBO
    #undef XMLOCAL_VBO
}
#endif

typedef struct GLTFNode {
    mat4 local; // local transform (relative to parent)
    mat4 scene; // scene-space transform
    struct GLTFNode* parent; // optional - may be a root node
} GLTFNode;

void ReadModelFromDisk (const char* name, Model* model, const char* gltfDirectory, const char* gltfFilename) {
    static char gltfPath [4096]; // path to GLTF file
    static char filePath [4096]; // buffer for storing other filenames
    stbsp_snprintf(gltfPath, vxSize(gltfPath), "%s/%s", gltfDirectory, gltfFilename);

    vxLog("Reading GLTF model from %s into 0x%jx...", gltfPath, model);
    JSON_Value* rootval = json_parse_file_with_comments(gltfPath);
    if (rootval == NULL) {
        vxLog("Failed to parse JSON file (unknown error in parson - does file exist?)");
    }
    JSON_Object* root = json_value_get_object(rootval);

    // We currently only support glTF 2.0 models:
    vxCheck(strcmp(json_object_dotget_string(root, "asset.version"), "2.0") == 0);

    // Extract buffers:
    JSON_Array* jbuffers = json_object_get_array(root, "buffers");
    size_t bufferCount   = json_array_get_count(jbuffers);
    size_t* bufferSizes  = vxAlloc(bufferCount, size_t);
    char** buffers       = vxAlloc(bufferCount, size_t);
    for (size_t ibuf = 0; ibuf < bufferCount; ibuf++) {
        JSON_Object* jbuf = json_array_get_object(jbuffers, ibuf);
        buffers[ibuf] = NULL;
        // TODO: This can be a data URI, maybe we should support that?
        const char* uri = json_object_get_string(jbuf, "uri");
        size_t len = (size_t) json_object_get_number(jbuf, "byteLength");
        if (uri && len) {
            // TODO: We should probably make this work with URIs like "../x.png" too.
            stbsp_snprintf(filePath, vxSize(filePath), "%s/%s", gltfDirectory, uri);
            bufferSizes[ibuf] = len;
            buffers[ibuf] = vxReadFile(filePath, "rb", NULL);
        }
        if (buffers[ibuf] == NULL) {
            bufferSizes[ibuf] = 0;
            vxLog("Warning: Failed to read buffer %ju from model.", ibuf);
        }
    }

    // Extract accessors:
    // TODO: Support min/max properties.
    // TODO: Support sparse accessors.
    // TODO: Support for integer value normalization, whatever that means.
    JSON_Array* jaccessors = json_object_get_array(root, "accessors");
    JSON_Array* jbufferviews = json_object_get_array(root, "bufferViews");
    size_t accessorCount = json_array_get_count(jaccessors);
    FAccessor* accessors = vxAlloc(accessorCount, FAccessor);
    for (size_t iacc = 0; iacc < accessorCount; iacc++) {
        JSON_Object* jacc = json_array_get_object(jaccessors, iacc);
        if (!json_object_has_value(jacc, "bufferView") || json_object_has_value(jacc, "sparse")) {
            vxLog("Warning: Sparse GLTF accessors are not supported.");
            vxLog("         Unable to load accessor %ju from model.", iacc);
        } else {
            size_t ibv = (size_t) json_object_get_number(jacc, "bufferView");
            JSON_Object* jbv = json_array_get_object(jbufferviews, ibv);
            // Retrieve accessor properties:
            const char* jtype   = json_object_get_string(jacc, "type");
            int jcomptype = (int) json_object_get_number(jacc, "componentType");
            int jcount    = (int) json_object_get_number(jacc, "count");
            int joffset1  = (int) json_object_get_number(jacc, "byteOffset"); // default 0
            // Retrive bufferview properties:
            int ibuf      = (int) json_object_get_number(jbv, "buffer");
            int joffset2  = (int) json_object_get_number(jbv, "byteOffset"); // default 0
            int jstride   = (int) json_object_get_number(jbv, "byteStride"); // default 0
            // Create accessor:
            FAccessorInit(&accessors[iacc], FAccessorTypeFromGltf(jtype, jcomptype),
                buffers[ibuf], (size_t)(joffset1 + joffset2), (size_t)(jcount), (uint8_t)(jstride));
        }
    }

    // Extract samplers:
    JSON_Array* jsamplers  = json_object_get_array(root, "samplers");
    size_t samplerCount    = json_array_get_count(jsamplers);
    GLuint* samplers       = vxAlloc(samplerCount, GLuint);
    bool* samplerNeedsMips = vxAlloc(samplerCount, bool);
    // Create GL sampler objects:
    glGenSamplers((GLsizei) samplerCount, samplers);
    // Set sampler parameters:
    for (size_t ismp = 0; ismp < samplerCount; ismp++) {
        JSON_Object* jsmp = json_array_get_object(jsamplers, ismp);
        // Retrieve:
        int minfilter = (int) json_object_get_number(jsmp, "minFilter");
        int magfilter = (int) json_object_get_number(jsmp, "magFilter");
        int wrapS = (int) json_object_get_number(jsmp, "wrapS");
        int wrapT = (int) json_object_get_number(jsmp, "wrapT");
        // Defaults:
        if (minfilter == 0) { minfilter = GL_LINEAR; }
        if (magfilter == 0) { magfilter = GL_LINEAR; }
        if (wrapS == 0) { wrapS = GL_REPEAT; }
        if (wrapT == 0) { wrapT = GL_REPEAT; }
        // Set: (GLTF uses OpenGL enums so we don't have to translate anything)
        glSamplerParameteri(samplers[ismp], GL_TEXTURE_MIN_FILTER, minfilter);
        glSamplerParameteri(samplers[ismp], GL_TEXTURE_MAG_FILTER, magfilter);
        glSamplerParameteri(samplers[ismp], GL_TEXTURE_WRAP_S, wrapS);
        glSamplerParameteri(samplers[ismp], GL_TEXTURE_WRAP_T, wrapT);
        // Determine whether this sampler needs mips:
        samplerNeedsMips[ismp] = false;
        if (minfilter == GL_NEAREST_MIPMAP_NEAREST ||
            minfilter == GL_NEAREST_MIPMAP_LINEAR  ||
            minfilter == GL_LINEAR_MIPMAP_NEAREST  ||
            minfilter == GL_LINEAR_MIPMAP_LINEAR) {
            samplerNeedsMips[ismp] = true;
        }
    }

    // Extract textures (i.e. GLTF images):
    JSON_Array* jimages = json_object_get_array(root, "images");
    JSON_Array* jtextures = json_object_get_array(root, "textures");
    size_t textureCount = json_array_get_count(jimages);
    size_t gltfTextureCount = json_array_get_count(jtextures);
    GLuint* textures = vxAlloc(textureCount, GLuint);
    // Create GL texture objects:
    glGenTextures(textureCount, textures);
    // Queue textures for read and upload:
    for (size_t iimg = 0; iimg < textureCount; iimg++) {
        JSON_Object* jimg = json_array_get_object(jimages, iimg);
        // Determine whether or not the texture needs mipmaps:
        bool needsMips = false;
        for (size_t itex = 0; itex < gltfTextureCount; itex++) {
            JSON_Object* jtex = json_array_get_object(jtextures, itex);
            if (json_object_has_value(jtex, "source") && json_object_has_value(jtex, "sampler")) {
                if ((int) json_object_get_number(jtex, "source") == iimg) {
                    int ismp = (int) json_object_get_number(jtex, "sampler");
                    if (samplerNeedsMips[ismp]) {
                        needsMips = true;
                        break;
                    }
                }
            }
        }
        // Queue texture:
        const char* uri = json_object_get_string(jimg, "uri");
        if (uri) {
            // TODO: We should probably make this work with URIs like "../x.png" too.
            stbsp_snprintf(filePath, vxSize(filePath), "%s/%s", gltfDirectory, uri);
            // FIXME: Implement asset queues
            #if 0
                QueueTextureLoad(textures[iimg], filePath, needsMips);
            #else
                textures[iimg] = LoadTextureFromDisk(uri, filePath);
            #endif
        } else {
            // TODO: Support reading images from buffers.
            vxLog("Warning: GLTF images stored in buffers are not supported.");
            vxLog("         Unable to load image %ju from model.", iimg);
        }
    }

    // Extract materials:
    JSON_Array* jmaterials = json_object_get_array(root, "materials");
    size_t materialCount   = json_array_get_count(jmaterials);
    Material* materials    = vxAlloc(materialCount, Material);
    for (size_t imat = 0; imat < materialCount; imat++) {
        Material* m = &materials[imat];
        InitMaterial(m);
        JSON_Object* jmat = json_array_get_object(jmaterials, imat);
        JSON_Object* jmr = json_object_get_object(jmat, "pbrMetallicRoughness");
        // Extract material factors:
        JSON_Array* jdiffusefac = json_object_get_array(jmr, "baseColorFactor");
        if (jdiffusefac) {
            m->const_diffuse[0] = (float) json_array_get_number(jdiffusefac, 0);
            m->const_diffuse[1] = (float) json_array_get_number(jdiffusefac, 1);
            m->const_diffuse[2] = (float) json_array_get_number(jdiffusefac, 2);
            m->const_diffuse[3] = (float) json_array_get_number(jdiffusefac, 3);
        }
        if (json_object_has_value(jmr, "metallicFactor")) {
            m->const_metallic = (float) json_object_get_number(jmr, "metallicFactor");
        }
        if (json_object_has_value(jmr, "roughnessFactor")) {
            m->const_roughness = (float) json_object_get_number(jmr, "roughnessFactor");
        }
        // Extract material textures:
        JSON_Object* jdiffusetex = json_object_get_object(jmr, "baseColorTexture");
        JSON_Object* jmetrghtex  = json_object_get_object(jmr, "metallicRoughnessTexture");
        JSON_Object* jnormaltex  = json_object_get_object(jmat, "normalTexture");
        JSON_Object* jocctex     = json_object_get_object(jmat, "occlusionTexture");
        if (jdiffusetex) {
            int itex = (int) json_object_get_number(jdiffusetex, "index");
            JSON_Object* jtex = json_array_get_object(jtextures, itex);
            if (jtex && json_object_has_value(jtex, "source") && json_object_has_value(jtex, "sampler")) {
                int iimg = (int) json_object_get_number(jtex, "source");
                int ismp = (int) json_object_get_number(jtex, "sampler");
                m->tex_diffuse = textures[iimg];
                m->smp_diffuse = samplers[ismp];
            }
        }
        if (jmetrghtex) {
            int itex = (int) json_object_get_number(jmetrghtex, "index");
            JSON_Object* jtex = json_array_get_object(jtextures, itex);
            if (jtex && json_object_has_value(jtex, "source") && json_object_has_value(jtex, "sampler")) {
                int iimg = (int) json_object_get_number(jtex, "source");
                int ismp = (int) json_object_get_number(jtex, "sampler");
                m->tex_occ_met_rgh = textures[iimg];
                m->smp_occ_met_rgh = samplers[ismp];
            }
        }
        if (jnormaltex) {
            int itex = (int) json_object_get_number(jnormaltex, "index");
            JSON_Object* jtex = json_array_get_object(jtextures, itex);
            if (jtex && json_object_has_value(jtex, "source") && json_object_has_value(jtex, "sampler")) {
                int iimg = (int) json_object_get_number(jtex, "source");
                int ismp = (int) json_object_get_number(jtex, "sampler");
                m->tex_normal = textures[iimg];
                m->smp_normal = samplers[ismp];
            }
        }
        if (jocctex) {
            int itex = (int) json_object_get_number(jocctex, "index");
            JSON_Object* jtex = json_array_get_object(jtextures, itex);
            if (jtex && json_object_has_value(jtex, "source") && json_object_has_value(jtex, "sampler")) {
                int iimg = (int) json_object_get_number(jtex, "source");
                int ismp = (int) json_object_get_number(jtex, "sampler");
                m->tex_occlusion = textures[iimg];
                m->smp_occlusion = samplers[ismp];
            }
        }
        // Extract alpha mode: (default is OPAQUE, i.e. no blending or stippling)
        const char* jalphamode = json_object_get_string(jmat, "alphaMode");
        if (json_object_has_value(jmat, "alphaCutoff")) {
            float jalphacutoff = (float) json_object_get_number(jmat, "alphaCutoff");
            m->stipple_hard_cutoff = jalphacutoff;
            m->stipple_soft_cutoff = jalphacutoff;
        }
        if (jalphamode) {
            if      (strcmp(jalphamode, "MASK")  == 0) { m->stipple = true; }
            else if (strcmp(jalphamode, "BLEND") == 0) { m->blend   = true; }
        }
        // Extract cull mode:
        bool jdoublesided = json_object_get_boolean(jmat, "doubleSided");
        if (jdoublesided) { m->cull = false; }
        vxLog("* Read material 0x%jx", m);
    }

    // Extract nodes and count meshes (GLTF primitives):
    JSON_Array* jnodes  = json_object_get_array(root, "nodes");
    JSON_Array* jmeshes = json_object_get_array(root, "meshes");
    size_t meshCount = 0;
    size_t nodeCount = json_array_get_count(jnodes);
    GLTFNode* nodes  = vxAlloc(nodeCount, GLTFNode);
    for (size_t inode = 0; inode < nodeCount; inode++) {
        GLTFNode* node = &nodes[inode];
        node->parent = NULL;
    }
    for (size_t inode = 0; inode < nodeCount; inode++) {
        GLTFNode* node = &nodes[inode];
        glm_mat4_identity(node->local);
        glm_mat4_identity(node->scene);
        JSON_Object* jnode = json_array_get_object(jnodes, inode);
        // Update this node's children:
        JSON_Array* jchildren = json_object_get_array(jnode, "children");
        if (jchildren) {
            for (size_t ichild = 0; ichild < json_array_get_count(jchildren); ichild++) {
                int ichildnode = (int) json_array_get_number(jchildren, ichild);
                nodes[ichildnode].parent = node;
            }
        }
        // Extract or generate local transform matrix:
        JSON_Array* jmat = json_object_get_array(jnode, "matrix");
        if (jmat) {
            // Both GLM and GLSL use column-major order for matrices, so just use a loop:
            for (int ipos = 0; ipos < 16; ipos++) {
                ((float*)node->local)[ipos] = (float) json_array_get_number(jmat, ipos);
            }
        } else {
            vec4 r = GLM_QUAT_IDENTITY_INIT;
            vec3 t = GLM_VEC3_ZERO_INIT;
            vec3 s = GLM_VEC3_ONE_INIT;
            JSON_Array* jr = json_object_get_array(jnode, "rotation");
            JSON_Array* jt = json_object_get_array(jnode, "translation");
            JSON_Array* js = json_object_get_array(jnode, "scale");
            if (jr) {
                r[0] = (float) json_array_get_number(jr, 0); // x
                r[1] = (float) json_array_get_number(jr, 1); // y
                r[2] = (float) json_array_get_number(jr, 2); // z
                r[3] = (float) json_array_get_number(jr, 3); // w
            }
            if (jt) {
                t[0] = (float) json_array_get_number(jt, 0);
                t[1] = (float) json_array_get_number(jt, 1);
                t[2] = (float) json_array_get_number(jt, 2);
            }
            if (js) {
                s[0] = (float) json_array_get_number(js, 0);
                s[1] = (float) json_array_get_number(js, 1);
                s[2] = (float) json_array_get_number(js, 2);
            }
            // FIXME: Is there a specific order this has to be done in?
            glm_translate(node->local, t);
            glm_quat_rotate(node->local, r, node->local);
            glm_scale(node->local, s);
        }
        // Count primitives:
        if (json_object_has_value(jnode, "mesh")) {
            int igltfmesh = (int) json_object_get_number(jnode, "mesh");
            JSON_Object* jmesh = json_array_get_object(jmeshes, igltfmesh);
            meshCount += json_array_get_count(json_object_get_array(jmesh, "primitives"));
        }
    }

    // Compute the scene-space transform matrix for each node:
    for (size_t inode = 0; inode < nodeCount; inode++) {
        GLTFNode* node = &nodes[inode];
        // The correct order is probably node.local * node.parent->scene for each node.
        // We'll do it iteratively instead of recursively, though.
        // FIXME: Is this actually right?
        glm_mat4_copy(node->local, node->scene);
        GLTFNode* parent = node->parent;
        while (parent != NULL) {
            glm_mat4_mul(node->scene, parent->local, node->scene); // correct order?
            parent = parent->parent;
        }
    }

    // Extract meshes (GLTF primitives) from the node structure:
    Mesh* meshes = vxAlloc(meshCount, Mesh);
    mat4* meshTransforms = vxAlloc(meshCount, mat4);
    size_t imesh = 0;
    for (size_t inode = 0; inode < nodeCount; inode++) {
        GLTFNode* node = &nodes[inode];
        JSON_Object* jnode = json_array_get_object(jnodes, inode);
        if (json_object_has_value(jnode, "mesh")) {
            int igltfmesh = (int) json_object_get_number(jnode, "mesh");
            JSON_Object* jmesh = json_array_get_object(jmeshes, igltfmesh);
            JSON_Array* jprims = json_object_get_array(jmesh, "primitives");
            for (size_t iprim = 0; iprim < json_array_get_count(jprims); iprim++) {
                // Store Mesh object for this primitive:
                JSON_Object* jprim = json_array_get_object(jprims, iprim);
                JSON_Object* jattr = json_object_get_object(jprim, "attributes");
                Mesh* mesh = &meshes[imesh];
                glGenVertexArrays(1, &mesh->gl_vertex_array);
                glm_mat4_copy(node->scene, meshTransforms[imesh]);
                // Debug:
                size_t meshVertexCount = 0;
                size_t meshIndexCount = 0;
                // Read type:
                mesh->type = GL_TRIANGLES;
                if (json_object_has_value(jprim, "mode")) {
                    mesh->type = (int) json_object_get_number(jprim, "mode");
                }
                // Read material:
                if (json_object_has_value(jprim, "material")) {
                    int imat = (int) json_object_get_number(jprim, "material");
                    mesh->material = &materials[imat];
                }
                // Read and upload indices:
                if (json_object_has_value(jprim, "indices")) {
                    int iacc = (int) json_object_get_number(jprim, "indices");
                    FAccessor* acc = &accessors[iacc];
                    glGenBuffers(1, &mesh->gl_element_array);
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->gl_element_array);
                    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                        (GLsizei)(acc->count * acc->stride), acc->buffer, GL_STATIC_DRAW);
                    mesh->gl_element_count = acc->count;
                    mesh->gl_element_type  = acc->type;
                    meshIndexCount += acc->count;
                }
                // Read and upload attributes:
                // FIXME: allow uploading something other than GL_FLOATs
                glBindVertexArray(mesh->gl_vertex_array);
                #define X(name, location, glslname, gltfname) { \
                    if (json_object_has_value(jattr, gltfname)) { \
                        int iacc = (int) json_object_get_number(jattr, gltfname); \
                        FAccessor* acc = &accessors[iacc]; \
                        GLuint vbo; \
                        glGenBuffers(1, &vbo); \
                        glBindBuffer(GL_ARRAY_BUFFER, vbo); \
                        glBufferData(GL_ARRAY_BUFFER, \
                            (GLsizei)(acc->count * acc->stride), acc->buffer, GL_STATIC_DRAW); \
                        glEnableVertexAttribArray(location); \
                        glVertexAttribPointer(location, FAccessorComponentCount(acc->type), \
                            GL_FLOAT, false, acc->stride, NULL); \
                        if (location == 0) { meshVertexCount += acc->count; } \
                    } \
                }
                XM_ATTRIBUTE_LOCATIONS
                #undef X
                vxLog("* Read mesh 0x%jx with %lu vertices, %lu indices, VAO %u, EBO %u", mesh,
                    meshVertexCount, meshIndexCount, mesh->gl_vertex_array, mesh->gl_element_array);
                imesh++;
            }
        }
    }
    meshCount = imesh;

    // Free temporary storage:
    vxFree(accessors);
    vxFree(buffers);
    vxFree(bufferSizes);
    vxFree(samplers);
    vxFree(samplerNeedsMips);
    vxFree(nodes);

    // Fill out model fields:
    // TODO: Add an atomic lock to the model so we can load it on another thread.
    model->textureCount = textureCount;
    model->textures = textures;
    model->materialCount = materialCount;
    model->materials = materials;
    model->meshCount = meshCount;
    model->meshTransforms = meshTransforms;
    model->meshes = meshes;
    vxLog("Uploaded model with %ju buffers, %ju materials and %ju meshes",
        bufferCount, materialCount, meshCount, gltfDirectory, gltfFilename);
}