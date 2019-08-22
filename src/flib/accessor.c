#include "accessor.h"

typedef struct {
    char* mem;
    size_t size;
} BufferCacheValue;

typedef struct {
    char* key;
    BufferCacheValue value;
} BufferCacheEntry;

typedef struct {
    FAccessor  key;
    FAccessor* value;
} AccessorCacheEntry;

bool S_CachesInitialized = false;
BufferCacheEntry*   S_BufferCache   = NULL;
AccessorCacheEntry* S_AccessorCache = NULL;

static void InitCaches() {
    if (!S_CachesInitialized) {
        S_CachesInitialized = true;
        stbds_sh_new_arena(S_BufferCache);
    }
}

char* FBufferFromFile (const char* filename, size_t* out_size) {
    char* buffer;
    InitCaches();
    ptrdiff_t i = stbds_shgeti(S_BufferCache, filename);
    size_t size;
    if (i >= 0) {
        buffer = S_BufferCache[i].value.mem;
        size   = S_BufferCache[i].value.size;
    } else {
        buffer = vxReadFile(filename, "rb", &size);
        vxCheck(buffer != NULL);
        BufferCacheValue v = {buffer, size};
        stbds_shput(S_BufferCache, filename, v);
    }
    if (out_size) {
        *out_size = size;
    }
    return buffer;
}

void FBufferFree (char* buffer) {
    InitCaches();
    for (size_t i = 0; i < stbds_shlenu(S_BufferCache); i++) {
        if (S_BufferCache[i].value.mem == buffer) {
            free(buffer);
            stbds_shdel(S_BufferCache, S_BufferCache[i].key);
            break;
        }
    }
}

void FAccessorInit (FAccessor* acc, FAccessorType t, void* buffer, size_t offset,
    size_t count, uint8_t stride)
{
    acc->buffer = (char*) buffer + offset;
    acc->count = count;
    acc->type = t;
    acc->stride = stride ? stride : FAccessorStride(t);
    acc->component_count = FAccessorComponentCount(t);
    acc->component_size  = FAccessorComponentSize(t);
    acc->gl_object = 0;
}

void FAccessorInitFile (FAccessor* acc, FAccessorType t, const char* filename, size_t offset,
    size_t count, uint8_t stride)
{
    char* buffer = FBufferFromFile(filename, NULL);
    FAccessorInit(acc, t, buffer, offset, count, stride);
}

FAccessor* FAccessorFromMemory (FAccessorType t, char* buffer, size_t offset,
    size_t count, uint8_t stride)
{
    FAccessor* cached;
    FAccessor acc;
    FAccessorInit(&acc, t, buffer, offset, count, stride);
    ptrdiff_t i = stbds_hmgeti(S_AccessorCache, acc);
    if (i >= 0) {
        cached = S_AccessorCache[i].value;
    } else {
        cached = vxAlloc(1, FAccessor);
        memcpy(cached, &acc, sizeof(FAccessor));
        stbds_hmput(S_AccessorCache, acc, cached);
    }
    return cached;
}

FAccessor* FAccessorFromFile (FAccessorType t, const char* filename, size_t offset,
    size_t count, uint8_t stride)
{
    char* buffer = FBufferFromFile(filename, NULL);
    return FAccessorFromMemory(t, buffer, offset, count, stride);
}