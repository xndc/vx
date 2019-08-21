#include <signal.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define VC_EXTRALEAN
    #define NOMINMAX
    #undef APIENTRY
    #include <windows.h>
    #include <shlwapi.h>
    #pragma comment(lib, "shlwapi.lib")
#elif __APPLE__
    #include <malloc/malloc.h>
    #include <sys/stat.h>
    // #include <mach-o/dyld.h>
    // #include <copyfile.h>
#else
    #include <malloc.h>
    #include <unistd.h>
    // #include <dlfcn.h>
#endif

#define STB_DS_IMPLEMENTATION
#define STB_SPRINTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "common.h"
#include <stb_image.h>
#include <glad/glad.c>

#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(push)
#pragma warning(disable:4232) // nonstandard extension: address of dllimport
#endif
#include <parson/parson.c>
#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(pop)
#endif

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
    if (vxLogBufUsed > 0 && (vxLogBufHashForLastFrame != vxLogBufHashForThisFrame)) {
        static char buf [256];
        int written = stbsp_snprintf(buf, 256, "Engine log for frame %ju:\n", vxFrameNumber);
        written += stbsp_snprintf(buf + written, 256 - written,
            "============================================================"
            "============================================================");
        buf[written] = 0;
        vxPutStrLn(buf);
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
        vxLogBufUsed += (size_t) stbsp_snprintf (vxLogBuf + vxLogBufUsed,
            (int)(vxLogBufSize - vxLogBufUsed - 2), "[%s] ", location);
        vxLogBufUsed += (size_t) stbsp_vsnprintf(vxLogBuf + vxLogBufUsed,
            (int)(vxLogBufSize - vxLogBufUsed - 2), fmt, va);
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
    vxLogBufHashForLastFrame = vxLogBufHashForThisFrame;
    vxLogBufHashForThisFrame = 0;
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
    #if 0
    // Convenience feature: if we're running in console mode, keep the console open.
    #ifdef _WIN32
    if (!IsDebuggerPresent() && GetConsoleWindow()) {
        printf("Press any key to quit the program.\n");
        getchar();
    }
    #endif
    #endif
    debug_break();
    abort(); // debug_break doesn't quit the app without a debugger attached, on some platforms
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

// Creates a directory. Does not create intermediate directories.
// TODO: Error handling, Mac/Linux implementation.
void vxCreateDirectory (const char* path) {
    #ifdef _WIN32
        CreateDirectoryA(path, NULL);
    #else
        mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    #endif
}

// Returns the last modification time for the given file or directory, or 0 if the given path does
// not point to a valid filesystem object.
uint64_t vxGetFileMtime (const char* path) {
    uint64_t t = 0;
    #if defined(_WIN32)
        WIN32_FIND_DATAA fd;
        HANDLE find = FindFirstFileA(path, &fd);
        if (find != INVALID_HANDLE_VALUE) {
            FILETIME ft = fd.ftLastWriteTime;
            ULARGE_INTEGER fti;
            fti.LowPart = ft.dwLowDateTime;
            fti.HighPart = ft.dwHighDateTime;
            t = (uint64_t) fti.QuadPart;
        }
    #else
        struct stat statbuf;
        if (stat(path, &statbuf) == 0) {
            time_t mtime = statbuf.st_mtime;
            t = (uint64_t) mtime;
        }
    #endif
    return t;
}

// Generic aligned_alloc, malloc_size and free functions.
static inline void* vxAlignedAlloc (size_t size, size_t alignment) {
    #if defined(_MSC_VER)
        return _aligned_malloc(size, alignment);
    #else
        // POSIX systems want the alignment to be a multiple of the system pointer size.
        while (alignment % sizeof(void*) != 0) {
            alignment *= 2;
        }
        #if defined(__APPLE__)
            void* p = NULL;
            posix_memalign(&p, alignment, size);
            return p;
        #else
            // POSIX/C11 aligned_alloc wants the size to be a multiple of the alignment:
            while (size % alignment != 0) {
                size *= 2;
            }
            return aligned_alloc(alignment, size);
        #endif
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
// default aligned memory allocator. The given alignment should be a power of 2.
// When freeing, the alignnment is not relevant on any of our supported platforms. Just pass 0.
void* vxAlignedRealloc (void* block, size_t count, size_t itemsize, size_t alignment) {
    void* p = NULL;
    size_t size = count * itemsize; // FIXME: check for overflow, somehow
    if (alignment == 0) {
        alignment = sizeof(void*);
    }

    #if 0
    // All systems want the alignment to be a power of 2:
    // https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
    alignment--;
    alignment |= alignment >> 1;
    alignment |= alignment >> 2;
    alignment |= alignment >> 4;
    alignment |= alignment >> 8;
    alignment |= alignment >> 16;
    alignment++;
    #endif

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