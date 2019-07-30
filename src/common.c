#include "common.h"
#include <string.h>
#include <time.h>
#include <malloc.h>

#define STB_SPRINTF_IMPLEMENTATION
#include <stb/stb_sprintf.h>
#define STB_DS_IMPLEMENTATION
#include <stb/stb_ds.h>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define VC_EXTRALEAN
    #define NOMINMAX
    #undef APIENTRY
    #include <Windows.h>
#endif

#include <glad/glad.c>

void vxVsprintf (size_t size, char* dst, const char* fmt, va_list args) {
    stbsp_vsnprintf(dst, (int) size, fmt, args);
}

void vxSprintf (size_t size, char* dst, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vxVsprintf(size, dst, fmt, args);
    va_end(args);
}

char* vxVsprintfStatic (const char* fmt, va_list args) {
    static char str [16 * VX_KiB];
    stbsp_vsnprintf(str, VXSIZE(str), fmt, args);
    return str;
}

char* vxSprintfStatic (const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char* str = vxVsprintfStatic(fmt, args);
    va_end(args);
    return str;
}

size_t vxLogBufferSize = 64 * VX_KiB;
size_t vxLogBufferUsed = 0;
char* vxLogBuffer = NULL;

void vxLogWrite (size_t size, const char* str) {
    size = strnlen(str, size == 0 ? vxLogBufferSize : size);
    if (vxLogBuffer == NULL) {
        vxLogBuffer = (char*) calloc(vxLogBufferSize, 1);
    }
    if (vxLogBufferUsed + size > vxLogBufferSize) {
        memcpy(&vxLogBuffer[0], &vxLogBuffer[vxLogBufferSize/2], vxLogBufferSize/2);
        vxLogBufferUsed = VXCLAMP(vxLogBufferUsed - vxLogBufferSize/2, 0, SIZE_MAX);
        memset(&vxLogBuffer[vxLogBufferUsed], 0, vxLogBufferSize - vxLogBufferUsed);
    }
    strncpy(&vxLogBuffer[vxLogBufferUsed], str, size);
    vxLogBufferUsed += size;

    fputs(str, stdout);
    #if !defined(NDEBUG) && defined(_WIN32)
        OutputDebugStringA(str);
    #endif
}

void vxLogVprintf (const char* fmt, va_list args) {
    char* msg = vxVsprintfStatic(fmt, args);
    vxLogWrite(0, msg);
}

void vxLogPrintf (const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vxLogVprintf(fmt, args);
    va_end(args);
}

void vxLogMessage (int source, const char* file, int line, const char* func, const char* fmt, ...) {
    const char* sourcename;
    switch (source) {
        case VX_LOGSOURCE_DEBUG: { sourcename = "[DEBUG]"; break; }
        case VX_LOGSOURCE_INFO:  { sourcename = "[INFO]";  break; }
        case VX_LOGSOURCE_WARN:  { sourcename = "[WARN]";  break; }
        case VX_LOGSOURCE_ERROR: { sourcename = "[ERROR]"; break; }
        case VX_LOGSOURCE_ALLOC: { sourcename = "[ALLOC]"; break; }
        default: { sourcename = "[????] "; }
    }

    time_t now = time(NULL);
    struct tm *localNow = localtime(&now);

    const char* s;
    s = strstr(file, "src/");  if (s != NULL) { file = s + 4; };
    s = strstr(file, "src\\"); if (s != NULL) { file = s + 4; };
    s = strstr(file, "lib/");  if (s != NULL) { file = s; };
    s = strstr(file, "lib\\"); if (s != NULL) { file = s; };
    s = strstr(file, "include/");  if (s != NULL) { file = s + 8; };
    s = strstr(file, "include\\"); if (s != NULL) { file = s + 8; };

    vxLogPrintf("[%02d:%02d:%02d] %s [%s:%d @%s] ",
        localNow->tm_hour, localNow->tm_min, localNow->tm_sec, sourcename, file, line, func);

    va_list args;
    va_start(args, fmt);
    vxLogVprintf(fmt, args);
    va_end(args);

    vxLogPrintf("\n");
}

char* vxStringCopy (size_t size, char* dst, const char* src) {
    strncpy(dst, src, size);
    return dst;
}

char* vxStringDuplicate (const char* src) {
    size_t len = strlen(src);
    char* dst = VXGENALLOC(len + 1, char);
    strncpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}

char* vxReadFileEx (size_t size, char* dst, size_t* out_read_bytes, FILE* file) {
    fseek(file, 0L, SEEK_END);
    size = VXCLAMP(ftell(file), 0, size);
    rewind(file);
    size_t read_bytes = fread(dst, sizeof(char), size, file);
    dst[read_bytes] = '\0';
    fclose(file);
    if (out_read_bytes != NULL) {
        *out_read_bytes = read_bytes;
    }
    return dst;
}

char* vxReadFile (const char* filename, bool text_mode, size_t* out_read_bytes) {
    FILE* file = NULL;
    if (text_mode) {
        file = fopen(filename, "r");
    } else {
        file = fopen(filename, "rb");
    }
    if (!file) {
        return NULL;
    }
    fseek(file, 0L, SEEK_END);
    size_t size = ftell(file);
    rewind(file);
    char* buffer = VXGENALLOC(size + 1, char);
    vxReadFileEx(size, buffer, out_read_bytes, file);
    return buffer;
}

#if 0
void* vxGenAllocEx (size_t count, size_t itemsize, size_t alignment, const char* file,
    int line, const char* func)
{
    if (count == 0 || itemsize == 0) {
        return NULL;
    } else {
        // Most systems want the alignment to be a power of 2 and a multiple of sizeof(void*).
        // https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
        size_t real_alignment = alignment;
        while (real_alignment % sizeof(void*) != 0) {
            real_alignment *= 2;
        }
        real_alignment--;
        real_alignment |= real_alignment >> 1;
        real_alignment |= real_alignment >> 2;
        real_alignment |= real_alignment >> 4;
        real_alignment |= real_alignment >> 8;
        real_alignment |= real_alignment >> 16;
        real_alignment++;
        vxLogMessage(VX_LOGSOURCE_ALLOC, file, line, func,
            "Allocating %jd items of size %jd with alignment %jd (real: %jd)",
            count, itemsize, alignment, real_alignment);
        #ifdef _MSC_VER
            void* mem = _aligned_malloc(count * itemsize, real_alignment);
        #else
            void* mem = aligned_alloc(real_alignment, count * itemsize);
        #endif
        return mem;
    }
}

void vxGenFreeEx (void* mem, const char* file, int line, const char* func) {
    if (mem != NULL) {
        #ifdef _MSC_VER
            _aligned_free(mem);
        #else
            free(mem);
        #endif
        vxLogMessage(VX_LOGSOURCE_ALLOC, file, line, func, "Freed block 0x%jx", mem);
    }
}
#endif

// Disables usage of _aligned_realloc on Windows, to test the generic alternative.
// #define VX_GEN_ALLOC_DISABLE_REALLOC

// Generic aligned_alloc, malloc_size and free functions.
#if defined(_MSC_VER)
    static inline void* SystemAlloc (size_t size, size_t alignment) {
        return _aligned_malloc(size, alignment);
    }
    static inline size_t SystemMemSize (void* block, size_t alignment) {
        return _aligned_msize(block, alignment, 0);
    }
    static inline void SystemFree (void* block) {
        _aligned_free(block);
    }
#else
    static inline void* SystemAlloc (size_t size, size_t alignment) {
        return aligned_alloc(size);
    }
    static inline void SystemMemSize (void* block, size_t alignment) {
        #ifdef __APPLE__
            return malloc_size(block);
        #else
            return malloc_usable_size(block);
        #endif
    }
    static inline void SystemFree (void* block) {
        free(block);
    }
#endif

void* vxGenAlloc (void* block, size_t count, size_t itemsize, size_t alignment,
    const char* file, int line, const char* func)
{
    void* p = NULL;
    size_t size = count * itemsize; // TODO: check for overflow, somehow

    if (alignment == 0) {
        alignment = itemsize;
    }
    #if !defined(VX_NO_ALLOC_MESSAGES)
        size_t given_alignment = alignment;
    #endif

    // Most systems want the alignment to be a power of 2 and a multiple of sizeof(void*).
    // https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
    while (alignment % sizeof(void*) != 0) {
        alignment *= 2;
    }
    alignment--;
    alignment |= alignment >> 1;
    alignment |= alignment >> 2;
    alignment |= alignment >> 4;
    alignment |= alignment >> 8;
    alignment |= alignment >> 16;
    alignment++;

    if (count == 0 || itemsize == 0) {
        if (block) {
            #if !defined(VX_NO_ALLOC_MESSAGES)
                vxLogMessage(VX_LOGSOURCE_ALLOC, file, line, func,
                "vxGenAlloc: Freeing block 0x%jx", block);
            #endif
            SystemFree(block);
        }
    } else {
        if (block) {
            #if !defined(VX_NO_ALLOC_MESSAGES)
                vxLogMessage(VX_LOGSOURCE_ALLOC, file, line, func,
                    "vxGenAlloc: Reallocating block 0x%jx to hold %jd * %jd bytes with alignment "
                    "%jd (given: %jd) ",
                    block, count, itemsize, alignment, given_alignment);
            #endif
            #if defined(_MSC_VER) && !defined(VX_GEN_ALLOC_DISABLE_REALLOC)
                p = _aligned_realloc(block, size, alignment);
            #else
                p = SystemAlloc(size, alignment);
                VXDEBUG("p = 0x%jx", p);
                // NOTE: This can return a bogus value on Windows if the alignments are different.
                size_t block_size = SystemMemSize(block, alignment);
                VXDEBUG("block_size = %jd", block_size);
                VXDEBUG("block = 0x%jx", block);
                VXDEBUG("copying %jd bytes", VXMIN(block_size, size));
                memcpy(p, block, VXMIN(block_size, size));
            #endif
            #if !defined(VX_NO_ALLOC_MESSAGES)
                vxLogMessage(VX_LOGSOURCE_ALLOC, file, line, func,
                    "vxGenAlloc: Reallocated block: 0x%jx", p);
            #endif
        } else {
            #if !defined(VX_NO_ALLOC_MESSAGES)
                vxLogMessage(VX_LOGSOURCE_ALLOC, file, line, func,
                    "vxGenAlloc: Allocating %jd items of size %jd with alignment %jd (given: %jd)",
                    count, itemsize, alignment, given_alignment);
            #endif
            p = SystemAlloc(size, alignment);
            #if !defined(VX_NO_ALLOC_MESSAGES)
                vxLogMessage(VX_LOGSOURCE_ALLOC, file, line, func,
                    "vxGenAlloc: Allocated block: 0x%jx", p);
            #endif
        }
    }
    return p;
}