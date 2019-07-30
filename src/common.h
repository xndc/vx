#pragma once
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#define _USE_MATH_DEFINES
#include <math.h>

#define VX_KiB 1024
#define VX_MiB 1048576
#define VX_GiB 1073741824

typedef int GLint;
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned int GLuint;

#define VXMIN(a, b) (((a) < (b))? (a) : (b))
#define VXMAX(a, b) (((a) > (b))? (a) : (b))
#define VXCLAMP(x, min, max) VXMIN(VXMAX(x, min), max)
#define VXSIZE(arr) (sizeof(arr)/sizeof(arr[0]))
#define VXRADIANS(deg) (deg * M_PI / 180.0)
#define VXDEGREES(rad) (rad * 180.0 / M_PI)

#ifdef _MSC_VER
    #define VXFUNCTION __FUNCTION__
#else
    #define VXFUNCTION __func__
#endif
#define VXLOCATION __FILE__, __LINE__, VXFUNCTION

#define VXDEBUG(...) vxLogMessage(0, VXLOCATION, __VA_ARGS__)
#define VXINFO(...)  vxLogMessage(1, VXLOCATION, __VA_ARGS__)
#define VXWARN(...)  vxLogMessage(2, VXLOCATION, __VA_ARGS__)
#define VXERROR(...) vxLogMessage(3, VXLOCATION, __VA_ARGS__)

#define VXABORT()     (void)(abort(), 0)
#define VXPANIC(...)  (void)(VXERROR(__VA_ARGS__), VXABORT(), 0)
#define VXCHECK(cond) (void)(!!(cond) || (VXPANIC("Check failed: %s", #cond), 0))
#define VXCHECKM(cond, ...) \
    (void)(!!(cond) || (VXPANIC(__VA_ARGS__), 0))

#ifndef NDEBUG
    #define VXASSERT(cond) (void)(!!(cond) || (VXPANIC("Assertion failed: %s", #cond), 0))
    #define VXASSERTM(cond, ...) \
        (void)(!!(cond) || (VXPANIC(__VA_ARGS__), 0))
#else
    #define VXASSERT(cond) (void)(0)
    #define VXASSERTM(cond, fmt, ...) (void)(0)
#endif

#define VX_LOGSOURCE_DEBUG 0
#define VX_LOGSOURCE_INFO  1
#define VX_LOGSOURCE_WARN  2
#define VX_LOGSOURCE_ERROR 3
#define VX_LOGSOURCE_ALLOC 100

void vxVsprintf (size_t size, char* dst, const char* fmt, va_list args);
void vxSprintf  (size_t size, char* dst, const char* fmt, ...);
char* vxSprintfStatic (const char* fmt, ...);

extern size_t vxLogBufferSize;
extern size_t vxLogBufferUsed;
extern char*  vxLogBuffer;
void vxLogWrite (size_t size, const char* str);
void vxLogPrintf (const char* fmt, ...);
void vxLogMessage (int source, const char* file, int line, const char* func, const char* fmt, ...);

char* vxStringDuplicate (const char* src);

char* vxReadFileEx (size_t size, char* dst, size_t* read_bytes, FILE* file);
char* vxReadFile (const char* filename, bool text_mode, size_t* read_bytes);

// Specification for a generic memory allocation/deallocation function.
// * block: Memory block to reallocate or free. Set to NULL to allocate a new block.
// * count: The number of "items" to allocate. Set this (or itemsize) to 0 to free a given block.
// * itemsize: The size in bytes of each "item". Set this (or count) to 0 to free a given block.
// * alignment: The alignment of the allocated block. Set this to 0 to align on itemsize.
// * file, line, func: Used for debug output. Use the VXLOCATION macro to fill these parameters.
typedef void* (*VXAllocator) (void* block, size_t count, size_t itemsize, size_t alignment,
    const char* file, int line, const char* func);

// Generic allocator, defers to platform allocation functions.
void* vxGenAlloc (void* block, size_t count, size_t itemsize, size_t alignment,
    const char* file, int line, const char* func);
#define VXGENALLOCA(count, type, align) \
    (type*) vxGenAlloc(NULL, count, sizeof(type), align, VXLOCATION);
#define VXGENALLOC(count, type) VXGENALLOCA(count, type, 0)
#define VXGENFREE(block) vxGenAlloc(block, 0, 0, 0, VXLOCATION);

// Per-frame linear allocator, should be reset using vxFrameAllocReset at the start of each frame.
void* vxFrameAlloc (void* block, size_t count, size_t itemsize, size_t alignment,
    const char* file, int line, const char* func);
void vxFrameAllocReset();
#define VXFRAMEALLOCA(count, type, align) \
    (type*) vxFrameAlloc(NULL, count, sizeof(type), align, VXLOCATION);
#define VXFRAMEALLOC(count, type) VXFRAMEALLOCA(count, type, 0)

#if 0
typedef void* (*VXAllocFunction) (size_t count, size_t itemsize, size_t alignment,
    const char* file, int line, const char* func);
typedef void (*VXFreeFunction) (void* mem, const char* file, int line, const char* func);

// General allocator: passes arguments to aligned_alloc, with logging.
void* vxGenAllocEx (size_t count, size_t itemsize, size_t alignment,
    const char* file, int line, const char* func);
void vxGenFreeEx (void* mem, const char* file, int line, const char* func);
#define VXGENALLOC(count, type) \
    (type*) vxGenAllocEx(count, sizeof(type), sizeof(type), __FILE__, __LINE__, VXFUNCTION)
#define VXGENFREE(mem) vxGenFreeEx(mem, __FILE__, __LINE__, VXFUNCTION);

// Frame allocator: linear allocator, should be reset every frame.
void* vxFrameAllocEx (size_t count, size_t itemsize, size_t alignment, const char* file,
    int line, const char* func);
void vxFrameAllocSize (size_t heapsize);
void vxFrameAllocReset ();
#define VXFRAMEALLOC(type) \
    (type*) vxFrameAllocEx(count, sizeof(type), sizeof(type), __FILE__, __LINE__, VXFUNCTION)
#endif