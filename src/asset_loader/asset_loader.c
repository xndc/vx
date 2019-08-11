#include "asset_loader.h"
#include "flib/ringbuffer.h"

typedef struct ProgQueueEntry {
    GLuint id;
    char* vshPath;
    char* fshPath;
} ProgQueueEntry;

typedef struct TexQueueEntry {
    GLuint id;
    char* path;
    GLenum target;
    bool mipmaps;
} TexQueueEntry;

FLIB_DECLARE_RINGBUFFER_STATIC(sProgQueue, ProgQueueEntry, 128);
FLIB_DECLARE_RINGBUFFER_STATIC(sTexQueue,  TexQueueEntry,  512);

GLuint QueueProgramLoad (char* vshPath, char* fshPath) {
    if (FRingBufferFull(sProgQueue)) {
        vxPanic("Program queue full! Can't add (%s, %s)", vshPath, fshPath);
    }
    ProgQueueEntry* entry = FRingBufferPush(sProgQueue);
    entry->id = glCreateProgram();
    entry->vshPath = strdup(vshPath);
    entry->fshPath = strdup(fshPath);
    return entry->id;
}

GLuint QueueTextureLoad (char* path, GLenum target, bool mipmaps) {
    if (FRingBufferFull(sTexQueue)) {
        vxPanic("Texture queue full! Can't add %s", path);
    }
    TexQueueEntry* entry = FRingBufferPush(sTexQueue);
    glCreateTextures(target, 1, &entry->id);
    entry->target = target;
    entry->path = strdup(path);
    entry->mipmaps = mipmaps;
    return entry->id;
}

void QueueDefaultAssets() {
    // Retrieve shader filenames:
    #define X(name, path) const char* name ## __PATH = path;
    XM_ASSETS_SHADERS
    #undef X

    #if 0
    // Queue programs:
    #define X(name, vsh, fsh) name = QueueProgramLoad(vsh ## __PATH, fsh ## __PATH);
    XM_ASSETS_PROGRAMS
    #undef X
    // Queue models:
    #define X(name, directory, filename) name = QueueModelLoad(directory, filename);
    XM_ASSETS_MODELS_GLTF
    #undef X
    #endif

    // Queue textures:
    #define X(name, target, mipmaps, path) name = QueueTextureLoad(path, target, mipmaps)
    XM_ASSETS_TEXTURES
    #undef X
}