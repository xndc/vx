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
        sh_new_arena(S_BufferCache);
    }
}

char* FBufferFromFile (const char* filename, size_t* out_size) {
    char* buffer;
    InitCaches();
    ptrdiff_t i = shgeti(S_BufferCache, filename);
    size_t size;
    if (i >= 0) {
        buffer = S_BufferCache[i].value.mem;
        size   = S_BufferCache[i].value.size;
    } else {
        buffer = vxReadFile(filename, false, &size);
        VXCHECK(buffer != NULL);
        BufferCacheValue v = {buffer, size};
        shput(S_BufferCache, filename, v);
    }
    if (out_size) {
        *out_size = size;
    }
    return buffer;
}

void FBufferFree (char* buffer) {
    InitCaches();
    for (size_t i = 0; i < shlenu(S_BufferCache); i++) {
        if (S_BufferCache[i].value.mem == buffer) {
            VXGENFREE(buffer);
            shdel(S_BufferCache, S_BufferCache[i].key);
            break;
        }
    }
}

void FAccessorInit (FAccessor* acc, FAccessorType t, char* buffer, size_t offset,
    size_t count, uint8_t stride)
{
    acc->buffer = buffer;
    acc->offset = offset;
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
    ptrdiff_t i = hmgeti(S_AccessorCache, acc);
    if (i >= 0) {
        cached = S_AccessorCache[i].value;
    } else {
        cached = VXGENALLOCA(1, FAccessor, sizeof(void*));
        memcpy(cached, &acc, sizeof(FAccessor));
        hmput(S_AccessorCache, acc, cached);
    }
    return cached;
}

FAccessor* FAccessorFromFile (FAccessorType t, const char* filename, size_t offset,
    size_t count, uint8_t stride)
{
    char* buffer = FBufferFromFile(filename, NULL);
    return FAccessorFromMemory(t, buffer, offset, count, stride);
}