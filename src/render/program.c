#include "program.h"

#define X(type, name, path) Shader name;
XM_SHADERS
#undef X

#define X(name, vsh, fsh) Program name;
XM_PROGRAMS
#undef X

#define X(name, glslName) GLint name;
XM_PROGRAM_UNIFORMS
#undef X

#define X(name, location, glslName, gltfName) const GLint name = location;
XM_PROGRAM_ATTRIBUTES
#undef X

typedef struct DefineBlock {
    uint64_t hash;
    char* defines;
} DefineBlock;

static DefineBlock sGenerateDefineBlock (vxConfig* conf) {
    static size_t hash = 0;
    static char block [4096];
    static bool blockExists = false;

    int i = 0;
    const int l = vxSize(block);
    bool fieldsChanged = false;

    #define DEFINE(type, field, block) \
        static type field; \
        if (!blockExists || field != conf->field) { fieldsChanged = true; field = conf->field; block; }
    #define WRITE(...) i += stbsp_snprintf(&block[i], l-i, __VA_ARGS__)
    
    DEFINE(bool, gpuSupportsClipControl, {
        if (gpuSupportsClipControl) { WRITE("#define DEPTH_ZERO_TO_ONE\n"); }
    });
    
    DEFINE(int, tonemapMode, {
        switch (tonemapMode) {
            case TONEMAP_LINEAR:   { WRITE("#define TONEMAP_LINEAR\n");   break; }
            case TONEMAP_REINHARD: { WRITE("#define TONEMAP_REINHARD\n"); break; }
            case TONEMAP_HABLE:    { WRITE("#define TONEMAP_HABLE\n");    break; }
            case TONEMAP_ACES:     { WRITE("#define TONEMAP_ACES\n");     break; }
        }
    });

    #define CASE(name) case name: { WRITE("#define DEBUG_VIS\n"); WRITE("#define " #name "\n"); } break;
    DEFINE(int, debugVisMode, {
        switch (debugVisMode) {
            CASE(DEBUG_VIS_GBUF_COLOR)
            CASE(DEBUG_VIS_GBUF_NORMAL)
            CASE(DEBUG_VIS_GBUF_ORM)
            CASE(DEBUG_VIS_GBUF_VELOCITY)
            CASE(DEBUG_VIS_WORLDPOS)
            CASE(DEBUG_VIS_DEPTH_RAW)
            CASE(DEBUG_VIS_DEPTH_LINEAR)
            CASE(DEBUG_VIS_SHADOWMAP)
        }
    });
    #undef CASE

    DEFINE(bool, debugShowLightVolumes, {
        if (debugShowLightVolumes) { WRITE("#define DEBUG_SHOW_LIGHT_VOLUMES"); }
    });

    DEFINE(int, shadowPcfTapsX, { WRITE("#define SHADOW_PCF_TAPS_X %d\n", conf->shadowPcfTapsX); });
    DEFINE(int, shadowPcfTapsY, { WRITE("#define SHADOW_PCF_TAPS_Y %d\n", conf->shadowPcfTapsY); });

    DEFINE(bool, shadowTAA, {
        if (shadowTAA) { WRITE("#define SHADOW_TAA\n"); }
    });

    DEFINE(bool, shadowNoise, {
        if (shadowNoise) { WRITE("#define SHADOW_NOISE\n"); }
    });

    #undef DEFINE
    #undef WRITE

    if (fieldsChanged) {
        // If one of the defines has changed, we obviously have to write all of them out. Doing a recursive call lets
        // us do that while having the relatively straightforward code structure you see above.
        if (blockExists) {
            blockExists = false;
            return sGenerateDefineBlock(conf);
        } else {
            blockExists = true;
        }
        block[i] = '\0';
        hash = stbds_hash_string(block, VX_SEED);
        vxLog("Generated new program define block:\n%s", block);
    }
    return (DefineBlock){hash, block};
}

static void sCompileShader (Shader* s, DefineBlock defineBlock) {
    char* code = vxReadFile(s->path, "r", NULL);
    if (strncmp(code, "#version", 8) != 0) {
        vxLog("Warning: shader %s has no #version declaration", s->path);
        return;
    } else {
        char* newline = strchr(code, '\n');
        s->codeBlock = newline + 1;
        size_t linesize = (size_t)(newline - code + 1);
        s->versionBlock = vxAlloc(linesize + 1, char);
        strncpy(s->versionBlock, code, linesize);
        s->versionBlock[linesize] = '\0';
    }
    const char* sources[] = {s->versionBlock, defineBlock.defines, s->codeBlock};

    GLuint shader = glCreateShader(s->type);
    glShaderSource(shader, (GLsizei) vxSize(sources), sources, NULL);
    s->defineBlockHash = defineBlock.hash;
    
    int ok, logsize;
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logsize);
    if (ok) {
        if (logsize == 0) {
            vxLog("Compiled shader %u (%s)", shader, s->path);
        } else {
            char* log = vxAlloc(logsize, char);
            glGetShaderInfoLog(shader, logsize, NULL, log);
            vxLog("Compiled shader %u (%s) with warnings:\n%s", shader, s->path, log);
            vxFree(log);
        }
        if (s->object != 0) {
            glDeleteShader(s->object);
        }
        s->object = shader;
        s->justReloaded = true;
    } else {
        char* log = vxAlloc(logsize, char);
        glGetShaderInfoLog(shader, logsize, NULL, log);
        vxLog("Failed to compile shader %s:\n%s", s->path, log);
        vxFree(log);
        glDeleteShader(shader);
    }
}

static void sLinkProgram (Program* p) {
    GLuint program = glCreateProgram();
    glAttachShader(program, p->vsh->object);
    glAttachShader(program, p->fsh->object);
    int ok, logsize;
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logsize);
    if (ok) {
        if (logsize == 0) {
            vxLog("Linked program %u (%s, %s)", program, p->vsh->path, p->fsh->path);
        } else {
            char* log = vxAlloc(logsize, char);
            glGetProgramInfoLog(program, logsize, NULL, log);
            vxLog("Linked program %u (%s, %s) with warnings:\n%s", program, p->vsh->path, p->fsh->path, log);
            vxFree(log);
        }
        if (p->object != 0) {
            glDeleteProgram(p->object);
        }
        p->object = program;
    } else {
        char* log = vxAlloc(logsize, char);
        glGetProgramInfoLog(program, logsize, NULL, log);
        vxLog("Warning: Failed to link program (%s, %s):\n%s", p->vsh->path, p->fsh->path, log);
        vxFree(log);
        glDeleteProgram(program);
    }
}

size_t    gShaderCount = 0;
Shader**  gShaders = NULL;
size_t    gProgramCount = 0;
Program** gPrograms = NULL;

static void sInitShader (size_t idx, Shader* s, const GLenum type, const char* path, DefineBlock defineBlock) {
    gShaders[idx] = s;
    s->object = 0;
    s->type = type;
    s->path = strdup(path);
    s->mtime = vxGetFileMtime(s->path);
    s->justReloaded = false;
    if (s->mtime == 0) {
        vxLog("Warning: shader %s could not be loaded from disk", s->path);
        s->versionBlock = NULL;
        s->codeBlock = NULL;
    } else {
        sCompileShader(s, defineBlock);
    }
}

static void sInitProgram (size_t idx, Program* p, Shader* vsh, Shader* fsh) {
    gPrograms[idx] = p;
    p->vsh = vsh;
    p->fsh = fsh;
    sLinkProgram(p);
}

// Loads all defined shaders into memory and compiles all defined render programs.
// Populates the shader and program globals (VSH_*, FSH_*, PROG_*).
void InitProgramSystem (vxConfig* conf) {
    #define X(type, name, path) gShaderCount++;
    XM_SHADERS
    #undef X

    #define X(name, vsh, fsh) gProgramCount++;
    XM_PROGRAMS
    #undef X

    gShaders  = vxAlloc(gShaderCount,  Shader*);
    gPrograms = vxAlloc(gProgramCount, Program*);

    DefineBlock defineBlock = sGenerateDefineBlock(conf);

    size_t iS = 0;
    #define X(type, name, path) sInitShader(iS++, &name, type, path, defineBlock);
    XM_SHADERS
    #undef X

    size_t iP = 0;
    #define X(name, vsh, fsh) sInitProgram(iP++, &name, &vsh, &fsh);
    XM_PROGRAMS
    #undef X
}

static void sUpdateShader (Shader* s, DefineBlock defineBlock) {
    uint64_t newMtime = vxGetFileMtime(s->path);
    if (newMtime != s->mtime || defineBlock.hash != s->defineBlockHash) {
        s->mtime = newMtime;
        sCompileShader(s, defineBlock);
    }
}

// Updates shaders and programs from disk. Should be called on each frame.
void UpdatePrograms (vxConfig* conf) {
    static enum {
        STEP_UNMARK_SHADERS,
        STEP_UPDATE_SHADERS,
        STEP_UPDATE_PROGRAMS,
    } step = STEP_UNMARK_SHADERS;

    if (step == STEP_UNMARK_SHADERS) {
        for (int i = 0; i < gShaderCount; i++) {
            Shader* s = gShaders[i];
            s->justReloaded = false;
        }
        step = STEP_UPDATE_SHADERS;
    } else if (step == STEP_UPDATE_SHADERS) {
        static int i = 0;
        Shader* s = gShaders[i];
        DefineBlock defineBlock = sGenerateDefineBlock(conf);
        sUpdateShader(s, defineBlock);
        if (++i >= gShaderCount) {
            i = 0;
            step = STEP_UPDATE_PROGRAMS;
        }
    } else if (step == STEP_UPDATE_PROGRAMS) {
        static int i = 0;
        Program* p = gPrograms[i];
        if ((p->vsh->justReloaded || p->fsh->justReloaded) &&
            (p->vsh->object != 0 && p->fsh->object != 0))
        {
            sLinkProgram(p);
        }
        if (++i >= gProgramCount) {
            i = 0;
            step = STEP_UNMARK_SHADERS;
        }
    }
}