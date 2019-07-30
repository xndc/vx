#include "accessor.h"
#include <stb/stb_ds.h>

// shgeti returns 0 instead of -1: https://github.com/nothings/stb/pull/781
#undef stbds_shgeti
#define stbds_shgeti(t,k) \
    ((t) = stbds_hmget_key_wrapper((t), sizeof *(t), (void*) (k), sizeof (t)->key, STBDS_HM_STRING), \
    stbds_temp(t-1))

typedef struct {
    char* key;
    char* value;
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

char* FBufferFromFile (const char* filename) {
    char* buffer;
    InitCaches();
    ptrdiff_t i = shgeti(S_BufferCache, filename);
    if (i >= 0) {
        buffer = S_BufferCache[i].value;
    } else {
        buffer = vxReadFile(filename, false, NULL);
        shput(S_BufferCache, filename, buffer);
    }
    return buffer;
}

FAccessor* FAccessorFromMemory (FAccessorType t, char* buffer, size_t offset,
    size_t count, size_t stride)
{
    FAccessor* cached;
    FAccessor acc;
    acc.buffer = buffer;
    acc.offset = offset;
    acc.count = count;
    acc.stride = stride ? stride : FAccessorStride(t);
    acc.component_count = FAccessorComponentCount(t);
    acc.component_size  = FAccessorComponentSize(t);
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
    size_t count, size_t stride)
{
    char* buffer = FBufferFromFile(filename);
    return FAccessorFromMemory(t, buffer, offset, count, stride);
}