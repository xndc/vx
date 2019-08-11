#include "common.h"
#include <signal.h>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define VC_EXTRALEAN
    #define NOMINMAX
    #undef APIENTRY
    #include <windows.h>
#endif

#ifdef __APPLE__
    #include <malloc/malloc.h>
#else
    #include <malloc.h>
#endif

#define STB_SPRINTF_IMPLEMENTATION
#include <stb_sprintf.h>
#define STB_DS_IMPLEMENTATION
#include <stb_ds.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glad/glad.c>
#include <parson/parson.c>

size_t vxFrameNumber = 0;
bool vxLogBufEnabled;
size_t vxLogBufSize = 64 * VX_KiB;
size_t vxLogBufUsed;
size_t vxLogBufHashForThisFrame;
size_t vxLogBufHashForLastFrame;
char* vxLogBuf = NULL;

// Initializes the frame log buffer. Should be run before using vxLogBuf in any way.
static void vxLogBufInit() {
    if (!vxLogBuf) {
        vxLogBuf = (char*) malloc(vxLogBufSize);
    }
}

// Configures the logging system.
extern void vxConfigureLogging() {
    vxLogBufInit();
    #ifdef _WIN32
    if (IsDebuggerPresent()) {
        FreeConsole(); // we don't need both a console and a debugger
    }
    #endif
}

static inline void vxPutStrLn (char* str) {
    #ifdef _WIN32
    if (IsDebuggerPresent()) {
        OutputDebugStringA(str);
        OutputDebugStringA("\n");
    }
    if (GetConsoleWindow()) {
        puts(str);
    }
    #else
    puts(str);
    #endif
}

// Prints out the current frame log buffer.
static void vxLogBufPrint() {
    vxLogBufInit();
    if (vxLogBufUsed > 0) {
        if (vxFrameNumber > 0) {
            static char buf [256];
            int written = stbsp_snprintf(buf, 256, "Engine log for frame %ju:\n", vxFrameNumber);
            written += stbsp_snprintf(buf + written, 256 - written,
                "================================================================================");
            buf[written] = 0;
            vxPutStrLn(buf);
        }
        vxPutStrLn(vxLogBuf);
    }
}

// Sets the current log destination to the frame log buffer.
void vxEnableLogBuffer() {
    vxLogBufEnabled = true;
}

// Sets the current log destination to stdout.
void vxDisableLogBuffer() {
    vxLogBufPrint();
    vxLogBufEnabled = false;
    vxLogBufUsed = 0;
    vxLogBuf[0] = 0;
}

// Prints a message to the current log destination (either stdout or the frame log buffer).
void vxLogPrint (const char* location, const char* fmt, ...) {
    vxLogBufInit();
    size_t initPos = vxLogBufUsed;
    // Strip long path prefixes from location:
    const char* s;
    s = strstr(location, "src/");  if (s != NULL) { location = s + 4; };
    s = strstr(location, "src\\"); if (s != NULL) { location = s + 4; };
    s = strstr(location, "lib/");  if (s != NULL) { location = s; };
    s = strstr(location, "lib\\"); if (s != NULL) { location = s; };
    s = strstr(location, "include/");  if (s != NULL) { location = s + 8; };
    s = strstr(location, "include\\"); if (s != NULL) { location = s + 8; };
    va_list va;
    va_start(va, fmt);
    if (vxLogBufEnabled) {
        // Log message to buffer:
        vxLogBufUsed += (size_t) stbsp_snprintf (vxLogBuf + vxLogBufUsed, vxLogBufSize - vxLogBufUsed - 2, "[%s] ", location);
        vxLogBufUsed += (size_t) stbsp_vsnprintf(vxLogBuf + vxLogBufUsed, vxLogBufSize - vxLogBufUsed - 2, fmt, va);
        vxLogBuf[vxLogBufUsed++] = '\n';
        vxLogBuf[vxLogBufUsed] = 0; // no ++, we want the NUL to be overwritten by the next print
        // Compute hash:
        vxLogBufHashForThisFrame ^= stbds_hash_bytes(vxLogBuf + initPos, vxLogBufUsed - initPos, VX_SEED);
    } else {
        // Log message to stdout:
        static char buf [1024];
        int written = 0;
        written += stbsp_snprintf (buf + written, 1024 - written - 2, "[%s] ", location);
        written += stbsp_vsnprintf(buf + written, 1024 - written - 2, fmt, va);
        buf[written] = 0;
        vxPutStrLn(buf);
    }
    va_end(va);
}

// Signals to the logging system that a new frame is starting. Prints the last frame's log buffer.
void vxAdvanceFrame() {
    vxDisableLogBuffer();
    vxEnableLogBuffer();
    vxFrameNumber++;
}

// Handles a signal by printing the log buffer and exiting.
VX_EXPORT void vxHandleSignal (int sig) {
    signal(sig, SIG_DFL);
    vxDisableLogBuffer();
    switch (sig) {
        case SIGABRT: { puts("Signal SIGABRT received."); } break;
        case SIGFPE:  { puts("Signal SIGFPE received.");  } break;
        case SIGILL:  { puts("Signal SIGILL received.");  } break;
        case SIGINT:  { puts("Signal SIGINT received.");  } break;
        case SIGSEGV: { puts("Signal SIGSEGV received."); } break;
        case SIGTERM: { puts("Signal SIGTERM received."); } break;
        default: { printf("Unknown signal %d received.", sig); }
    }
    // Convenience feature: if we're running in console mode, keep the console open.
    #ifdef _WIN32
    if (!IsDebuggerPresent() && GetConsoleWindow()) {
        printf("Press any key to quit the program.\n");
        getchar();
    }
    #endif
    debug_break();
    abort(); // debug_break doesn't quit the apps without a debugger attached, on some platforms
}

// Register signal handlers.
// Required if you want debug output to be printed in case of early termination.
void vxEnableSignalHandlers() {
    signal(SIGABRT, vxHandleSignal);
    signal(SIGFPE,  vxHandleSignal);
    signal(SIGILL,  vxHandleSignal);
    signal(SIGINT,  vxHandleSignal);
    signal(SIGSEGV, vxHandleSignal);
    signal(SIGTERM, vxHandleSignal);
}

// Reads a file from disk into memory, returning a null-terminated buffer.
// Writes the file's length, not including the final NUL, to [outLength] if provided.
// The returned buffer can be released using free().
char* vxReadFile (const char* filename, const char* mode, size_t* outLength) {
    char* buf = NULL;
    FILE* file = fopen(filename, mode);
    if (!file) {
        vxLog("Warning: couldn't read file %s (%s)", filename, strerror(errno));
    } else {
        fseek(file, 0L, SEEK_END);
        size_t size = ftell(file);
        if (outLength) {
            *outLength = size;
        }
        rewind(file);
        buf = (char*) malloc(size + 1);
        // NOTE: due to newline conversion, we might read less than size bytes
        size_t read = fread(buf, sizeof(char), size, file);
        fclose(file);
        buf[read] = 0;
    }
    return buf;
}

// Generic aligned_alloc, malloc_size and free functions.
static inline void* vxAlignedAlloc (size_t size, size_t alignment) {
    #if defined(_MSC_VER)
        return _aligned_malloc(size, alignment);
    #elif defined(__APPLE__)
        void* p;
        posix_memalign(&p, alignment, size);
        return p;
    #else
        return aligned_alloc(alignment, size);
    #endif
}
static inline size_t vxAlignedMemSize (void* block, size_t alignment) {
    #if defined(_MSC_VER)
        return _aligned_msize(block, alignment, 0);
    #elif defined(__APPLE__)
        return malloc_size(block);
    #else
        return malloc_usable_size(block);
    #endif
}
static inline void vxAlignedFree (void* block) {
    #if defined(_MSC_VER)
        _aligned_free(block);
    #else
        free(block);
    #endif
}

// Allocates (block=NULL), reallocates or frees (count*size = 0) a block of memory using the system
// default aligned memory allocator. The given alignment should be a power of 2 and a multiple of
// the system pointer size.
void* vxAlignedRealloc (void* block, size_t count, size_t itemsize, size_t alignment) {
    void* p = NULL;
    size_t size = count * itemsize; // FIXME: check for overflow, somehow
    if (alignment == 0) { alignment = sizeof(void*); }

    if (size == 0) {
        if (block) {
            vxAlignedFree(block);
        }
    } else {
        if (block) {
            #if defined(_MSC_VER)
                p = _aligned_realloc(block, size, alignment);
            #else
                p = vxAlignedAlloc(size, alignment);
                // NOTE: This can return a bogus value on Windows if the alignments are different.
                size_t block_size = vxAlignedMemSize(block, alignment);
                memcpy(p, block, vxMin(block_size, size));
            #endif
        } else {
            p = vxAlignedAlloc(size, alignment);
        }
    }
    return p;
}

#if 0
#ifdef __APPLE__
    #include <malloc/malloc.h>
#else
    #include <malloc.h>
#endif

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
    stbsp_vsnprintf(str, vxSize(str), fmt, args);
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
        vxLogBufferUsed = vxClamp(vxLogBufferUsed - vxLogBufferSize/2, 0, SIZE_MAX);
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
    size = vxClamp(ftell(file), 0, size);
    rewind(file);
    size_t read_bytes = fread(dst, sizeof(char), size, file);
    dst[read_bytes] = '\0';
    if (out_read_bytes != NULL) {
        *out_read_bytes = read_bytes;
    }
    rewind(file);
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
        vxLog("Warning: Failed to read file %s (%s)", filename, strerror(errno));
        return NULL;
    }
    fseek(file, 0L, SEEK_END);
    size_t size = ftell(file);
    rewind(file);
    char* buffer = VXGENALLOC(size + 1, char);
    vxReadFileEx(size, buffer, out_read_bytes, file);
    fclose(file);
    return buffer;
}

// Disables usage of _aligned_realloc on Windows, to test the generic alternative.
// #define VX_GEN_ALLOC_DISABLE_REALLOC

// Generic aligned_alloc, malloc_size and free functions.
static inline void* vxAlignedAlloc (size_t size, size_t alignment) {
    #if defined(_MSC_VER)
        return _aligned_malloc(size, alignment);
    #elif defined(__APPLE__)
        void* p;
        posix_memalign(&p, alignment, size);
        return p;
    #else
        return aligned_alloc(alignment, size);
    #endif
}
static inline size_t SystemMemSize (void* block, size_t alignment) {
    #if defined(_MSC_VER)
        return _aligned_msize(block, alignment, 0);
    #elif defined(__APPLE__)
        return malloc_size(block);
    #else
        return malloc_usable_size(block);
    #endif
}
static inline void SystemFree (void* block) {
    #if defined(_MSC_VER)
        _aligned_free(block);
    #else
        free(block);
    #endif
}

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
                p = vxAlignedAlloc(size, alignment);
                vxDebug("p = 0x%jx", p);
                // NOTE: This can return a bogus value on Windows if the alignments are different.
                size_t block_size = SystemMemSize(block, alignment);
                vxDebug("block_size = %jd", block_size);
                vxDebug("block = 0x%jx", block);
                vxDebug("copying %jd bytes", vxMin(block_size, size));
                memcpy(p, block, vxMin(block_size, size));
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
            p = vxAlignedAlloc(size, alignment);
            #if !defined(VX_NO_ALLOC_MESSAGES)
                vxLogMessage(VX_LOGSOURCE_ALLOC, file, line, func,
                    "vxGenAlloc: Allocated block: 0x%jx", p);
            #endif
        }
    }
    return p;
}
#endif