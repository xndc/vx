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
    for (size_t i = 0; i < bufferCount; i++) {
        free(buffers[i]); // allocated by vxReadFile using malloc
    }
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