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

// Remotery is a profiler that runs in Chrome and connects to our program via WebSocket.
// Note that Remotery's OpenGL support conflicts with our Unity-style stats display.
#undef RMT_ENABLED
#undef RMT_USE_OPENGL
#define RMT_ENABLED 0
#define RMT_USE_OPENGL 0
#include <Remotery.h>

// stb_ds trips some warnings and has one bug that we have to fix:
#if defined(_MSC_VER) && !defined(__clang__)
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
#if defined(_MSC_VER) && !defined(__clang__)
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

#define VX_UP_INIT {0.0f, 1.0f, 0.0f}
#define VX_UP ((vec3) VX_UP_INIT)

#define VX_KiB 1024
#define VX_MiB 1048576
#define VX_GiB 1073741824
// Randomly generated 32-bit value, for use with hash functions:
#define VX_SEED 4105099677U

#if defined(_MSC_VER) && !defined(__clang__)
    #define VX_LOCATION __FILE__ ":" VX_STRINGIFY(__LINE__) " @" __FUNCTION__
#else
    // NOTE: __func__ can't be concatenated with anything else, at least on Clang
    #define VX_LOCATION __FILE__ ":" VX_STRINGIFY(__LINE__)
#endif

// Logging, assertions, global frame counter:

extern size_t vxFrameNumber;
extern void vxAdvanceFrame();

extern bool vxLogBufEnabled;
extern size_t vxLogBufSize;
extern size_t vxLogBufUsed;
extern size_t vxLogBufHashForThisFrame;
extern size_t vxLogBufHashForLastFrame;
extern char* vxLogBuf;

VX_EXPORT void vxConfigureLogging();
VX_EXPORT void vxEnableLogBuffer();
VX_EXPORT void vxDisableLogBuffer();

VX_EXPORT void vxLogPrint(const char* location, const char* fmt, ...);
#define vxLog(...) vxLogPrint(VX_LOCATION, __VA_ARGS__)
#ifdef NDEBUG
    #define vxDebug(...) vxLog(__VA_ARGS__)
#else
    #define vxDebug(...)
#endif

VX_EXPORT void vxEnableSignalHandlers();
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

// File IO:

VX_EXPORT char* vxReadFile (const char* filename, const char* mode, size_t* outLength);
VX_EXPORT uint64_t vxGetFileMtime (const char* path);
VX_EXPORT char** vxListFiles (const char* directory, const char* pattern);
VX_EXPORT void vxCreateDirectory (const char* path);

// Memory management:

#if defined(_MSC_VER) && !defined(__clang__)
    #define vxAlignOf(t) __alignof(t)
#else
    #include <stdalign.h>
    #define vxAlignOf(t) alignof(t)
#endif

VX_EXPORT void* vxAlignedRealloc (void* block, size_t count, size_t itemsize, size_t alignment);
#define vxAlloc(count, type) (type*) vxAlignedRealloc(NULL, count, sizeof(type), vxAlignOf(type));
#define vxFree(block) vxAlignedRealloc(block, 0, 0, 0);

// Profiler instrumentation:
// We're using Remotery now, but that can change at any time.

#define StartBlock(name) rmt_BeginCPUSampleDynamic(name, 0)
#define EndBlock() rmt_EndCPUSample()

#define StartGPUBlock(name) { rmt_BeginOpenGLSampleDynamic(name); StartBlock(name); }
#define EndGPUBlock() { EndBlock(); rmt_EndOpenGLSample(); }

#define TimedBlock(name, block)    { StartBlock(name); block; EndBlock(); }
#define TimedGPUBlock(name, block) { StartGPUBlock(name); block; EndGPUBlock(); }