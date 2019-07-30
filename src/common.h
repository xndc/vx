#pragma once
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
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

#define VXDEBUG(...) vxLogMessage(0, __FILE__, __LINE__, VXFUNCTION, __VA_ARGS__)
#define VXINFO(...)  vxLogMessage(1, __FILE__, __LINE__, VXFUNCTION, __VA_ARGS__)
#define VXWARN(...)  vxLogMessage(2, __FILE__, __LINE__, VXFUNCTION, __VA_ARGS__)
#define VXERROR(...) vxLogMessage(3, __FILE__, __LINE__, VXFUNCTION, __VA_ARGS__)

#define VXPANIC(...)  (void)(VXERROR(__VA_ARGS__), abort(), 0)
#define VXCHECK(cond) (void)(!!(cond) || (VXPANIC("Check failed: %s", #cond), 0))
#define VXCHECKM(cond, ...) \
    (void)(!!(cond) || (VXPANIC(__VA_ARGS__), 0))

#ifndef NDEBUG
    #define ASSERT(cond) (void)(!!(cond) || (VXPANIC("Assertion failed: %s", #cond), 0))
    #define ASSERTM(cond, ...) \
        (void)(!!(cond) || (VXPANIC(__VA_ARGS__), 0))
#else
    #define ASSERT(cond) (void)(0)
    #define ASSERTM(cond, fmt, ...) (void)(0)
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