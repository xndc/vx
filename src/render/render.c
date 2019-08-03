#include "render.h"
#include <glad/glad.h>
#include <stb/stb_sprintf.h>

#define X(name, path) Shader* name = NULL;
XM_ASSETS_SHADERS
#undef X

#define X(name, glsl_name) GLint name = -1;
XM_ASSETS_SHADER_ATTRIBUTES
XM_ASSETS_SHADER_UNIFORMS
#undef X

typedef struct {
    char name [64];
    char text [128];
} ShaderDefine;

typedef struct {
    ShaderDefine* defines;
    Shader* current_vsh;
    Shader* current_fsh;
    bool defines_changed;
    GLuint program;
} RenderState;

typedef struct {
    Shader* vsh;
    Shader* fsh;
} ProgramCacheKey;

typedef struct {
    ShaderDefine* defines;
    GLuint program;
} ProgramCacheValue;

typedef struct {
    ProgramCacheKey key;
    ProgramCacheValue value;
} ProgramCacheEntry;

RenderState S_RenderState = {0};
RenderState* S_RenderStateStack = NULL;
ProgramCacheEntry* S_ProgramCache = NULL;

static Shader* LoadShaderFromDisk (const char* name, const char* path) {
    Shader* shader = VXGENALLOC(1, Shader);
    shader->name = name;
    char* code = vxReadFile(path, true, NULL);
    shader->full = code;
    if (strncmp(code, "#version", 8) == 0) {
        char* newline = strchr(code, '\n');
        shader->body = newline + 1;
        size_t linesize = (size_t)(newline - code + 1);
        shader->version = VXGENALLOC(linesize + 1, char);
        strncpy(shader->version, code, linesize);
        shader->version[linesize] = '\0';
    } else {
        VXPANIC("Shader %s (%s) has no #version declaration", name, path);
    }
    VXINFO("Loaded shader %s from %s", name, path);
    return shader;
}

void InitRenderSystem() {
    #define X(name, path) name = LoadShaderFromDisk(#name, path);
    XM_ASSETS_SHADERS
    #undef X
}

// Conceptually, starts a new render pass. Concretely, resets the following render state variables:
// * the currently selected shaders
// * the current shader #define set
// * the current uniform and attribute locations (UNIF_*, ATTR_*)
// * any texture and sampler bindings
// This function does NOT reset the following:
// * the currently bound framebuffer
// This function also signals to the debug and timing systems that a new render pass is starting.
void StartRenderPass (const char* name) {
    ResetShaderDefines();
    // ResetShaderVariables();
    // ResetTextureUnits();
    S_RenderState = (RenderState){0};
    arrfree(S_RenderStateStack);
}

void PushRenderState() {
    arrput(S_RenderStateStack, S_RenderState);
}

void PopRenderState() {
    if (arrlen(S_RenderStateStack) > 0) {
        S_RenderState = arrlast(S_RenderStateStack);
        arrpop(S_RenderStateStack);
    }
}

static GLuint CompileShader (const char* name, GLenum type, size_t nsources, const char** sources) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, (GLsizei) nsources, sources, NULL);
    int ok, logsize;
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &logsize);
    if (logsize > 0) {
        char* log = VXGENALLOC(logsize, char);
        glGetShaderInfoLog(sh, logsize, NULL, log);
        VXINFO("Compilation log for shader %s:\n%s", name, log);
        VXGENFREE(log);
    }
    if (ok) {
        VXINFO("Compiled vertex shader %s into OpenGL shader object %u", name, sh);
    } else {
        VXPANIC("Failed to compile vertex shader %s", name);
    }
    return sh;
}

static GLuint LinkProgram (const char* vsh_name, GLuint vsh, const char* fsh_name, GLuint fsh) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vsh);
    glAttachShader(program, fsh);
    int ok, logsize;
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logsize);
    if (logsize > 0) {
        char* log = VXGENALLOC(logsize, char);
        glGetProgramInfoLog(program, logsize, NULL, log);
        VXINFO("Link log for program (%s, %s):\n%s", vsh_name, fsh_name, log);
        VXGENFREE(log);
    }
    if (ok) {
        VXINFO("Linked program (%s, %s) into OpenGL object %u", vsh_name, fsh_name, program);
    } else {
        VXPANIC("Failed to link program (%s, %s)", vsh_name, fsh_name);
    }
    return program;
}

void SetRenderProgram (Shader* vsh, Shader* fsh) {
    GLuint program = S_RenderState.program;
    if (vsh == NULL || fsh == NULL) {
        program = 0;
        vsh = NULL;
        fsh = NULL;
    } else {
        if (vsh != S_RenderState.current_vsh) { program = 0; }
        if (fsh != S_RenderState.current_fsh) { program = 0; }
        if (program == 0 || S_RenderState.defines_changed) {
            // Cache lookup:
            ProgramCacheKey key = {vsh, fsh};
            ptrdiff_t pidx = hmgeti(S_ProgramCache, key);
            ProgramCacheValue* v = (pidx >= 0) ? &S_ProgramCache[pidx].value : NULL;
            bool match = (v != NULL);
            if (match && arrlen(v->defines) == arrlen(S_RenderState.defines)) {
                for (size_t i = 0; i < arrlenu(v->defines); i++) {
                    if (strcmp(v->defines[i].name, S_RenderState.defines[i].name) != 0 ||
                        strcmp(v->defines[i].text, S_RenderState.defines[i].text) != 0)
                    {
                        match = false;
                        break;
                    }
                }
            }
            if (match) {
                program = v->program;
            }
        }
        if (program == 0) {
            ShaderDefine* defines = S_RenderState.defines;
            // Generate #define block:
            static char define_block [16 * VX_KiB];
            size_t cursor_pos = 0;
            for (size_t i = 0; i < arrlenu(defines); i++) {
                char* cursor = &define_block[cursor_pos];
                size_t space = VXSIZE(define_block) - cursor_pos;
                size_t written = stbsp_snprintf(cursor, (int) space, "#define %s %s\n",
                    defines[i].name, defines[i].text);
                VXCHECK(written > 0);
                cursor_pos += written;
            }
            define_block[cursor_pos] = '\0';
            // Compile shaders and link program:
            const char* vs_src[] = {vsh->version, define_block, vsh->body};
            const char* fs_src[] = {fsh->version, define_block, fsh->body};
            GLuint glvs = CompileShader(vsh->name, GL_VERTEX_SHADER,   VXSIZE(vs_src), vs_src);
            GLuint glfs = CompileShader(fsh->name, GL_FRAGMENT_SHADER, VXSIZE(fs_src), fs_src);
            program = LinkProgram(vsh->name, glvs, fsh->name, glfs);
            // Save program to cache:
            ProgramCacheKey key = {vsh, fsh};
            ProgramCacheValue v = {NULL, program};
            for (size_t i = 0; i < arrlenu(defines); i++) {
                arrput(v.defines, defines[i]);
            }
            hmput(S_ProgramCache, key, v);
        }
    }
    // Retrieve uniform and attribute locations:
    if (program != 0) {
        #define X(name, glsl_name) name = glGetUniformLocation(program, glsl_name);
        XM_ASSETS_SHADER_UNIFORMS
        #undef X
        #define X(name, glsl_name) name = glGetAttribLocation(program, glsl_name);
        XM_ASSETS_SHADER_ATTRIBUTES
        #undef X
    } else {
        #define X(name, glsl_name) name = -1;
        XM_ASSETS_SHADER_UNIFORMS
        XM_ASSETS_SHADER_ATTRIBUTES
        #undef X
    }
    S_RenderState.program = program;
    S_RenderState.current_vsh = vsh;
    S_RenderState.current_fsh = fsh;
    S_RenderState.defines_changed = false;
    glUseProgram(program);
}

void AddShaderDefine (const char* name, const char* text) {
    ShaderDefine def = {0};
    strcpy(def.name, name);
    strcpy(def.text, text);
    arrput(S_RenderState.defines, def);
    S_RenderState.defines_changed = true;
}

void ResetShaderDefines() {
    arrfree(S_RenderState.defines);
}