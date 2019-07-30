#pragma once
#include <common.h>
#include <flib/array.h>

typedef struct {
    char* name;
    char* text;
} ProgramDefine;

// typedef struct {
//     size_t count;
//     size_t alloc;
//     ProgramDefine* defines;
// } ProgramDefineList;

// void InitProgramDefineList (ProgramDefineList* list, size_t reserve);
// void FreeProgramDefineList (ProgramDefineList* list);
// void CopyProgramDefineList (ProgramDefineList* dst, ProgramDefineList* src);
// void AddProgramDefine (ProgramDefineList* list, const char* name, const char* text);

void AddProgramDefine (FArray* array, const char* name, const char* text);

GLuint GetProgram (const char* vert_path, const char* frag_path, FArray* defines);