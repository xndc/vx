#pragma once
#include "common.h"
#include "assets.h"

typedef struct Shader {
    char* path;
    uint64_t mtime;
    GLenum type;
    char* versionBlock;
    char* codeBlock;
    GLuint object;
    bool justReloaded;
} Shader;

typedef struct Program {
    Shader* vsh;
    Shader* fsh;
    GLuint object;
} Program;

#define X(type, name, path) extern Shader name;
XM_SHADERS
#undef X

#define X(name, vsh, fsh) extern Program name;
XM_PROGRAMS
#undef X

#define X(name, glslName) extern GLint name;
XM_PROGRAM_UNIFORMS
#undef X

#define X(name, location, glslName, gltfName) extern const GLint name;
XM_PROGRAM_ATTRIBUTES
#undef X

void InitProgramSystem();
void UpdatePrograms();

extern size_t    gShaderCount;
extern Shader**  gShaders;
extern size_t    gProgramCount;
extern Program** gPrograms;