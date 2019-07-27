#include "common.h"
#include <string.h>
#include <time.h>
#define STB_SPRINTF_IMPLEMENTATION
#include <stb/stb_sprintf.h>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define VC_EXTRALEAN
    #define NOMINMAX
    #undef APIENTRY
    #include <Windows.h>
#endif

#include <glad/glad.h>

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

void* vxGenAllocEx (size_t count, size_t itemsize, size_t alignment, const char* file,
    int line, const char* func)
{
    #ifdef _MSC_VER
        void* mem = _aligned_malloc(count * itemsize, alignment);
    #else
        void* mem = aligned_alloc(alignment, count * itemsize);
    #endif
    vxLogMessage(VX_LOGSOURCE_ALLOC, file, line, func,
        "Allocated %ld items of size %ld with alignment %ld => 0x%lx",
        count, itemsize, alignment, mem);
    return mem;
}