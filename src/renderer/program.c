#include "program.h"
#include <glad/glad.h>
#include <stb/stb_sprintf.h>
#include <flib/array.h>

// void InitProgramDefineList (ProgramDefineList* list, size_t reserve) {
//     list->count = 0;
//     list->alloc = reserve;
//     list->defines = VXGENALLOC(reserve, ProgramDefine);
// }

// void FreeProgramDefineList (ProgramDefineList* list) {
//     for (size_t i = 0; i < list->count; i++) {
//         VXGENFREE(list->defines[i].name);
//         VXGENFREE(list->defines[i].text);
//     }
//     VXGENFREE(list->defines);
// }

// void CopyProgramDefineList (ProgramDefineList* dst, ProgramDefineList* src) {
//     InitProgramDefineList(dst, src->count);
//     dst->count = src->count;
//     for (size_t i = 0; i < src->count; i++) {
//         dst->defines[i].name = vxStringDuplicate(src->defines[i].name);
//         dst->defines[i].text = vxStringDuplicate(src->defines[i].text);
//     }
// }

// void AddProgramDefine (ProgramDefineList* list, const char* name, const char* text) {
//     list->count += 1;
//     if (list->count > list->alloc) {
//         list->alloc *= 2;
//         ProgramDefine* old_defines = list->defines;
//         list->defines = VXGENALLOC(list->alloc, ProgramDefine);
//         memcpy(list->defines, old_defines, (list->count - 1) * sizeof(ProgramDefine));
//         VXGENFREE(old_defines);
//     }
//     list->defines[list->count].name = vxStringDuplicate(name);
//     list->defines[list->count].text = vxStringDuplicate(text);
// }

void AddProgramDefine (FArray* array, const char* name, const char* text) {
    ProgramDefine* def = FArrayReserve(array, 1);
    def->name = vxStringDuplicate(name);
    def->text = vxStringDuplicate(text);
}

typedef struct {
    char* path;
    char* code;
} Shader;

typedef struct {
    Shader vert;
    Shader frag;
    FArray defines;
    GLuint program;
} ProgramCacheEntry;

FArray S_ProgramCache = {0};

GLuint GetProgram (const char* vert_path, const char* frag_path, FArray* defines) {
    GLuint program = 0;
    // Initialize cache:
    if (S_ProgramCache.capacity == 0) {
        FArrayInit(&S_ProgramCache, sizeof(ProgramCacheEntry), 64, NULL, NULL, NULL);
    }
    // Cache lookup:
    int found_cache_entry = -1;
    for (size_t i = 0; i < S_ProgramCache.size; i++) {
        ProgramCacheEntry e = ((ProgramCacheEntry*) S_ProgramCache.items)[i];
        if (strcmp(e.vert.path, vert_path) != 0) break;
        if (strcmp(e.frag.path, frag_path) != 0) break;
        if (e.defines.size != defines->size) break;
        bool ok = true;
        for (size_t j = 0; j < e.defines.size; j++) {
            ProgramDefine req = ((ProgramDefine*)defines->items)[j];
            ProgramDefine pcd = ((ProgramDefine*)e.defines.items)[j];
            if (strcmp(req.name, pcd.name) != 0) { ok = false; break; }
            if (strcmp(req.text, pcd.text) != 0) { ok = false; break; }
        }
        if (ok) {
            found_cache_entry = (int) i;
            break;
        }
    }
    if (found_cache_entry >= 0) {
        program = ((ProgramCacheEntry*) S_ProgramCache.items)[found_cache_entry].program;
    } else {
        // Compile, link and cache new program:
        ProgramCacheEntry* pce = FArrayReserve(&S_ProgramCache, 1);
        FArrayInit(&pce->defines, sizeof(ProgramDefine), defines->size, NULL, NULL, NULL);
        FArrayPush(&pce->defines, defines->size, defines->items);
        // Read shaders from disk:
        pce->vert.path = vxStringDuplicate(vert_path);
        pce->vert.code = vxReadFile(vert_path, true, NULL);
        VXCHECKM(pce->vert.code != NULL, "Failed to read vertex shader code from %s", vert_path);
        pce->frag.path = vxStringDuplicate(frag_path);
        pce->frag.code = vxReadFile(frag_path, true, NULL);
        VXCHECKM(pce->frag.code != NULL, "Failed to read fragment shader code from %s", frag_path);
        // Copy the #version line from each shader into a buffer, then trim it from the code that
        // we're going to pass to the shader compiler:
        char* vs = pce->vert.code;
        char* fs = pce->frag.code;
        static char vs_version[32];
        static char fs_version[32];
        static const char default_version[] = "#version 330 core";
        if (strncmp(vs, "#version", VXSIZE("#version") - 1) == 0) {
            char* newline = strchr(vs, '\n');
            size_t linesize = (size_t)(newline - vs + 1);
            strncpy(vs_version, vs, linesize);
            vs_version[linesize] = '\0';
            vs = newline + 1;
        } else {
            VXWARN("Shader %s has no #version block, adding %s", vert_path, default_version);
            strcpy(vs_version, default_version);
            vs_version[VXSIZE(default_version) + 0] = '\n';
            vs_version[VXSIZE(default_version) + 1] = '\0';
        }
        if (strncmp(fs, "#version", VXSIZE("#version") - 1) == 0) {
            char* newline = strchr(fs, '\n');
            size_t linesize = (size_t)(newline - fs + 1);
            strncpy(fs_version, fs, linesize);
            fs_version[linesize] = '\0';
            fs = newline + 1;
        } else {
            VXWARN("Shader %s has no #version block, adding %s", frag_path, default_version);
            strcpy(fs_version, default_version);
            fs_version[VXSIZE(default_version) + 0] = '\n';
            fs_version[VXSIZE(default_version) + 1] = '\0';
        }
        // Generate #define block:
        static char define_block [16 * VX_KiB];
        size_t cursor_pos = 0;
        for (size_t i = 0; i < defines->size; i++) {
            char* cursor = &define_block[cursor_pos];
            size_t space = VXSIZE(define_block) - cursor_pos;
            size_t written = stbsp_snprintf(cursor, (int) space, "#define %s %s\n",
                ((ProgramDefine*)defines->items)[i].name,
                ((ProgramDefine*)defines->items)[i].text);
            VXCHECK(written > 0);
            cursor_pos += written;
        }
        define_block[cursor_pos] = '\0';
        // Compile shaders:
        const char* vs_src[] = {vs_version, define_block, vs};
        const char* fs_src[] = {fs_version, define_block, fs};
        GLuint glvs = glCreateShader(GL_VERTEX_SHADER);
        GLuint glfs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(glvs, VXSIZE(vs_src), vs_src, NULL);
        glShaderSource(glfs, VXSIZE(fs_src), fs_src, NULL);
        /* Vertex shader: */ {
            int vs_ok, vs_logsize;
            glCompileShader(glvs);
            glGetShaderiv(glvs, GL_COMPILE_STATUS, &vs_ok);
            glGetShaderiv(glvs, GL_INFO_LOG_LENGTH, &vs_logsize);
            if (vs_logsize > 0) {
                char* log = VXGENALLOC(vs_logsize, char);
                glGetShaderInfoLog(glvs, vs_logsize, NULL, log);
                VXINFO("Compilation log for shader %s:\n%s", vert_path, log);
                VXGENFREE(log);
            }
            if (vs_ok) {
                VXINFO("Compiled vertex shader %s into OpenGL shader object %u", vert_path, glvs);
            } else {
                VXPANIC("Failed to compile vertex shader %s", vert_path);
            }
        }
        /* Fragment shader: */ {
            int fs_ok, fs_logsize;
            glCompileShader(glfs);
            glGetShaderiv(glfs, GL_COMPILE_STATUS, &fs_ok);
            glGetShaderiv(glfs, GL_INFO_LOG_LENGTH, &fs_logsize);
            if (fs_logsize > 0) {
                char* log = VXGENALLOC(fs_logsize, char);
                glGetShaderInfoLog(glfs, fs_logsize, NULL, log);
                VXINFO("Compilation log for shader %s:\n%s", frag_path, log);
                VXGENFREE(log);
            }
            if (fs_ok) {
                VXINFO("Compiled fragment shader %s into OpenGL shader object %u", frag_path, glfs);
            } else {
                VXPANIC("Failed to compile fragment shader %s", frag_path);
            }
        }
        // Compile program:
        program = glCreateProgram();
        glAttachShader(program, glvs);
        glAttachShader(program, glfs);
        {
            int ok, logsize;
            glLinkProgram(program);
            glGetProgramiv(program, GL_LINK_STATUS, &ok);
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logsize);
            if (logsize > 0) {
                char* log = VXGENALLOC(logsize, char);
                glGetProgramInfoLog(program, logsize, NULL, log);
                VXINFO("Link log for program (%s, %s):\n%s", vert_path, frag_path, log);
                VXGENFREE(log);
            }
            if (ok) {
                VXINFO("Linked program (%s, %s) with %d defines into OpenGL program object %u",
                    vert_path, frag_path, defines->size, program);
            } else {
                VXPANIC("Failed to link program (%s, %s) with %d defines",
                    vert_path, frag_path, defines->size);
            }
        }
    }
    return program;
}