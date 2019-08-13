#pragma once

// Common header containing functionality required by a large portion of game TUs.
// We include things like cglm and the OpenGL headers here, although that's probably not the best
// idea. It shouldn't hurt our build times too much, though.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <errno.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include <debug_break.h>
#include <cglm/cglm.h>
#include <glad/glad.h>
#include <stb_sprintf.h>

// stb_ds trips some warnings and has one bug that we have to fix:
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4456)
#pragma warning(disable:4244)
#pragma warning(disable:4702)
#endif
#define STBDS_NO_SHORT_NAMES
#include <stb_ds.h>
// WORKAROUND: stbds_shgeti returns 0 instead of -1: https://github.com/nothings/stb/pull/781
#undef stbds_shgeti
#define stbds_shgeti(t,k) \
    ((t) = stbds_hmget_key_wrapper((t), sizeof *(t), (void*) (k), sizeof (t)->key, STBDS_HM_STRING), \
    stbds_temp(t-1))
#ifdef _MSC_VER
#pragma warning(pop)
#endif

// Generally useful macros and constants:
#ifdef __cplusplus
    #define VX_EXPORT extern "C"
#else
    #define VX_EXPORT
#endif

#define VX_STRINGIFY_HELPER(x) #x
#define VX_STRINGIFY(x) VX_STRINGIFY_HELPER(x)

#define vxMin(a, b) (((a) < (b))? (a) : (b))
#define vxMax(a, b) (((a) > (b))? (a) : (b))
#define vxClamp(x, min, max) vxMin(vxMax(x, min), max)
#define vxSize(arr) (sizeof(arr)/sizeof(arr[0]))
#define vxRadians(deg) (deg * M_PI / 180.0)
#define vxDegrees(rad) (rad * 180.0 / M_PI)

#define VX_KiB 1024
#define VX_MiB 1048576
#define VX_GiB 1073741824
 // Randomly generated 32-bit value, for use with hash functions:
#define VX_SEED 4105099677U

#ifdef _MSC_VER
    #define VX_LOCATION __FILE__ ":" VX_STRINGIFY(__LINE__) " @" __FUNCTION__
#else
    // NOTE: __func__ can't be concatenated with anything else, at least on Clang
    #define VX_LOCATION __FILE__ ":" VX_STRINGIFY(__LINE__)
#endif

extern size_t vxFrameNumber;
extern void vxAdvanceFrame();

extern bool vxLogBufEnabled;
extern size_t vxLogBufSize;
extern size_t vxLogBufUsed;
extern size_t vxLogBufHashForThisFrame;
extern size_t vxLogBufHashForLastFrame;
extern char* vxLogBuf;

extern void vxConfigureLogging();
extern void vxEnableLogBuffer();
extern void vxDisableLogBuffer();

extern void vxLogPrint(const char* location, const char* fmt, ...);
#define vxLog(...) vxLogPrint(VX_LOCATION, __VA_ARGS__)
#ifdef NDEBUG
    #define vxDebug(...) vxLog(__VA_ARGS__)
#else
    #define vxDebug(...)
#endif

extern void vxEnableSignalHandlers();
#define vxPanic(...) do { \
    vxDisableLogBuffer(); \
    vxLog(__VA_ARGS__); \
    debug_break(); \
    abort(); \
} while(0)
#define vxCheck(cond) do { if (!(cond)) { \
    vxDisableLogBuffer(); \
    vxPanic("Check failed: " #cond); \
}} while(0)
#define vxCheckMsg(cond, ...) do { if (!(cond)) { \
    vxDisableLogBuffer(); \
    vxLog("Check failed: " #cond); \
    vxPanic(__VA_ARGS__); \
}} while(0)
#ifndef NDEBUG
    #define vxAssert(...) vxCheck(__VA_ARGS__)
    #define vxAssertMsg(...) vxCheckMsg(__VA_ARGS__)
#else
    #define vxAssert(...)
    #define vxAssertMsg(...)
#endif

char* vxReadFile (const char* filename, const char* mode, size_t* outLength);
uint64_t vxGetFileMtime (const char* path);

#ifdef _MSC_VER
    #define vxAlignOf(t) __alignof(t)
#else
    #include <stdalign.h>
    #define vxAlignOf(t) alignof(t)
#endif

extern void* vxAlignedRealloc (void* block, size_t count, size_t itemsize, size_t alignment);
#define vxAlloc(count, type) (type*) vxAlignedRealloc(NULL, count, sizeof(type), vxAlignOf(type));
#define vxFree(block) vxAlignedRealloc(block, 0, 0, 0);

#if 0
#ifdef _MSC_VER
    #define VXFUNCTION __FUNCTION__
#else
    #define VXFUNCTION __func__
#endif
#define VXLOCATION __FILE__, __LINE__, VXFUNCTION

#define vxLog(...)  vxLogMessage(1, VXLOCATION, __VA_ARGS__)
#define VXWARN(...)  vxLogMessage(2, VXLOCATION, __VA_ARGS__)
#define VXERROR(...) vxLogMessage(3, VXLOCATION, __VA_ARGS__)

#define VXABORT()     (void)(abort(), 0)
#define vxPanic(...)  (void)(VXERROR(__VA_ARGS__), VXABORT(), 0)
#define VXCHECK(cond) (void)(!!(cond) || (vxPanic("Check failed: %s", #cond), 0))
#define VXCHECKM(cond, ...) \
    (void)(!!(cond) || (vxPanic(__VA_ARGS__), 0))

#ifndef NDEBUG
    #define vxDebug(...) vxLogMessage(0, VXLOCATION, __VA_ARGS__)
    #define VXASSERT(cond) (void)(!!(cond) || (vxPanic("Assertion failed: %s", #cond), 0))
    #define VXASSERTM(cond, ...) \
        (void)(!!(cond) || (vxPanic(__VA_ARGS__), 0))
#else
    #define vxDebug(...) (void)(0)
    #define VXASSERT(cond) (void)(0)
    #define VXASSERTM(cond, fmt, ...) (void)(0)
#endif

#define VX_LOGSOURCE_DEBUG 0
#define VX_LOGSOURCE_INFO  1
#define VX_LOGSOURCE_WARN  2
#define VX_LOGSOURCE_ERROR 3
#define VX_LOGSOURCE_ALLOC 100

VX_EXPORT void vxVsprintf (size_t size, char* dst, const char* fmt, va_list args);
VX_EXPORT void vxSprintf  (size_t size, char* dst, const char* fmt, ...);
VX_EXPORT char* vxSprintfStatic (const char* fmt, ...);

VX_EXPORT extern size_t vxLogBufferSize;
VX_EXPORT extern size_t vxLogBufferUsed;
VX_EXPORT extern char*  vxLogBuffer;
VX_EXPORT void vxLogWrite (size_t size, const char* str);
VX_EXPORT void vxLogPrintf (const char* fmt, ...);
VX_EXPORT void vxLogMessage (int source, const char* file, int line, const char* func, const char* fmt, ...);

VX_EXPORT char* vxStringDuplicate (const char* src);

VX_EXPORT char* vxReadFileEx (size_t size, char* dst, size_t* read_bytes, FILE* file);
VX_EXPORT char* vxReadFile (const char* filename, bool text_mode, size_t* read_bytes);

// Specification for a generic memory allocation/deallocation function.
// * block: Memory block to reallocate or free. Set to NULL to allocate a new block.
// * count: The number of "items" to allocate. Set this (or itemsize) to 0 to free a given block.
// * itemsize: The size in bytes of each "item". Set this (or count) to 0 to free a given block.
// * alignment: The alignment of the allocated block. Set this to 0 to align on itemsize.
// * file, line, func: Used for debug output. Use the VXLOCATION macro to fill these parameters.
typedef void* (*VXAllocator) (void* block, size_t count, size_t itemsize, size_t alignment,
    const char* file, int line, const char* func);

// Generic allocator, defers to platform allocation functions.
VX_EXPORT void* vxGenAlloc (void* block, size_t count, size_t itemsize, size_t alignment,
    const char* file, int line, const char* func);
#define VXGENALLOCA(count, type, align) \
    (type*) vxGenAlloc(NULL, count, sizeof(type), align, VXLOCATION);
#define VXGENALLOC(count, type) VXGENALLOCA(count, type, 0)
#define VXGENFREE(block) vxGenAlloc(block, 0, 0, 0, VXLOCATION);

#if 0
// Per-frame linear allocator, should be reset using vxFrameAllocReset at the start of each frame.
void* vxFrameAlloc (void* block, size_t count, size_t itemsize, size_t alignment,
    const char* file, int line, const char* func);
void vxFrameAllocReset();
#define VXFRAMEALLOCA(count, type, align) \
    (type*) vxFrameAlloc(NULL, count, sizeof(type), align, VXLOCATION);
#define VXFRAMEALLOC(count, type) VXFRAMEALLOCA(count, type, 0)
#endif
#endif