#pragma once
#include "common.h"

typedef enum {
    FACCESSOR_UINT8,
    FACCESSOR_SINT8,
    FACCESSOR_UINT16,
    FACCESSOR_SINT16,
    FACCESSOR_UINT32,
    FACCESSOR_SINT32,
    FACCESSOR_FLOAT32,

    FACCESSOR_UINT8_VEC2,
    FACCESSOR_SINT8_VEC2,
    FACCESSOR_UINT16_VEC2,
    FACCESSOR_SINT16_VEC2,
    FACCESSOR_UINT32_VEC2,
    FACCESSOR_SINT32_VEC2,
    FACCESSOR_FLOAT32_VEC2,

    FACCESSOR_UINT8_VEC3,
    FACCESSOR_SINT8_VEC3,
    FACCESSOR_UINT16_VEC3,
    FACCESSOR_SINT16_VEC3,
    FACCESSOR_UINT32_VEC3,
    FACCESSOR_SINT32_VEC3,
    FACCESSOR_FLOAT32_VEC3,

    FACCESSOR_UINT8_VEC4,
    FACCESSOR_SINT8_VEC4,
    FACCESSOR_UINT16_VEC4,
    FACCESSOR_SINT16_VEC4,
    FACCESSOR_UINT32_VEC4,
    FACCESSOR_SINT32_VEC4,
    FACCESSOR_FLOAT32_VEC4,

    FACCESSOR_UINT8_MAT2,
    FACCESSOR_SINT8_MAT2,
    FACCESSOR_UINT16_MAT2,
    FACCESSOR_SINT16_MAT2,
    FACCESSOR_UINT32_MAT2,
    FACCESSOR_SINT32_MAT2,
    FACCESSOR_FLOAT32_MAT2,

    FACCESSOR_UINT8_MAT3,
    FACCESSOR_SINT8_MAT3,
    FACCESSOR_UINT16_MAT3,
    FACCESSOR_SINT16_MAT3,
    FACCESSOR_UINT32_MAT3,
    FACCESSOR_SINT32_MAT3,
    FACCESSOR_FLOAT32_MAT3,

    FACCESSOR_UINT8_MAT4,
    FACCESSOR_SINT8_MAT4,
    FACCESSOR_UINT16_MAT4,
    FACCESSOR_SINT16_MAT4,
    FACCESSOR_UINT32_MAT4,
    FACCESSOR_SINT32_MAT4,
    FACCESSOR_FLOAT32_MAT4,
} FAccessorType;

static inline uint8_t FAccessorComponentCount (FAccessorType t) {
    if (t >= FACCESSOR_UINT8      && t <= FACCESSOR_FLOAT32)      { return 1; }
    if (t >= FACCESSOR_UINT8_VEC2 && t <= FACCESSOR_FLOAT32_VEC2) { return 2; }
    if (t >= FACCESSOR_UINT8_VEC3 && t <= FACCESSOR_FLOAT32_VEC3) { return 3; }
    if (t >= FACCESSOR_UINT8_VEC4 && t <= FACCESSOR_FLOAT32_VEC4) { return 4; }
    if (t >= FACCESSOR_UINT8_MAT2 && t <= FACCESSOR_FLOAT32_MAT2) { return 4; }
    if (t >= FACCESSOR_UINT8_MAT3 && t <= FACCESSOR_FLOAT32_MAT3) { return 9; }
    if (t >= FACCESSOR_UINT8_MAT4 && t <= FACCESSOR_FLOAT32_MAT4) { return 16; }
    VXPANIC("FAccessor type %d is invalid", t);
}

static inline uint8_t FAccessorComponentSize (FAccessorType t) {
    switch (t % FACCESSOR_UINT8_VEC2) {
        case FACCESSOR_UINT8:
        case FACCESSOR_SINT8:
            return 1;
        case FACCESSOR_UINT16:
        case FACCESSOR_SINT16:
            return 2;
        case FACCESSOR_UINT32:
        case FACCESSOR_SINT32:
        case FACCESSOR_FLOAT32:
            return 4;
    }
    VXPANIC("FAccessor type %d is invalid", t);
}

static inline uint8_t FAccessorStride (FAccessorType t) {
    return FAccessorComponentCount(t) * FAccessorComponentSize(t);
}

static inline FAccessorType FAccessorTypeFromGltf (const char* type, uint32_t component_type) {
    #define CASES_SCALAR() \
        case 5120: return FACCESSOR_SINT8;  \
        case 5121: return FACCESSOR_UINT8;  \
        case 5122: return FACCESSOR_SINT16; \
        case 5123: return FACCESSOR_UINT16; \
        case 5125: return FACCESSOR_UINT32; \
        case 5126: return FACCESSOR_FLOAT32;
    #define CASES_VECTOR(VTYPE) \
        case 5120: return FACCESSOR_SINT8_   ## VTYPE; \
        case 5121: return FACCESSOR_UINT8_   ## VTYPE; \
        case 5122: return FACCESSOR_SINT16_  ## VTYPE; \
        case 5123: return FACCESSOR_UINT16_  ## VTYPE; \
        case 5125: return FACCESSOR_UINT32_  ## VTYPE; \
        case 5126: return FACCESSOR_FLOAT32_ ## VTYPE;
    if (strcmp(type, "SCALAR") == 0)    { switch (component_type) { CASES_SCALAR() } }
    else if (strcmp(type, "VEC2") == 0) { switch (component_type) { CASES_VECTOR(VEC2) } }
    else if (strcmp(type, "VEC3") == 0) { switch (component_type) { CASES_VECTOR(VEC3) } }
    else if (strcmp(type, "VEC4") == 0) { switch (component_type) { CASES_VECTOR(VEC4) } }
    else if (strcmp(type, "MAT2") == 0) { switch (component_type) { CASES_VECTOR(MAT2) } }
    else if (strcmp(type, "MAT3") == 0) { switch (component_type) { CASES_VECTOR(MAT3) } }
    else if (strcmp(type, "MAT4") == 0) { switch (component_type) { CASES_VECTOR(MAT4) } }
    VXPANIC("GLTF types (%s, %d) don't match any FAccessorType", type, component_type);
}

typedef struct {
    FAccessorType type;
    char* buffer;
    size_t offset;
    size_t count;
    uint8_t stride;
    uint8_t component_count;
    uint8_t component_size;
    GLuint gl_object;
} FAccessor;

// Reads a file into memory or returns a previously read version of it.
char* FBufferFromFile (const char* filename, size_t* out_size);

// Frees a previously allocated buffer.
void FBufferFree (char* buffer);

// Initializes an accessor, given an in-memory buffer.
void FAccessorInit (FAccessor* acc, FAccessorType t, char* buffer, size_t offset,
    size_t count, uint8_t stride);

// Initializes an accessor, given a filename.
void FAccessorInitFile (FAccessor* acc, FAccessorType t, const char* filename, size_t offset,
    size_t count, uint8_t stride);

// Creates or retrieves a previously created FAccessor, given an in-memory buffer.
FAccessor* FAccessorFromMemory (FAccessorType t, char* buffer, size_t offset,
    size_t count, uint8_t stride);

// Creates or retrieves a previously created FAccessor, given a filename.
FAccessor* FAccessorFromFile (FAccessorType t, const char* filename, size_t offset,
    size_t count, uint8_t stride);

// Retrieves a pointer to the data the accessor is actually pointing to.
static inline char* FAccessorData (FAccessor* a) {
    if (a == NULL) { return NULL; }
    return a->buffer + a->offset;
}
static inline char* FAccessorElement (FAccessor* a, size_t index, size_t component) {
    if (a == NULL) { return NULL; }
    if (index > a->count) { return NULL; }
    if (component > a->component_count) { return NULL; }
    return a->buffer + a->offset + (index * a->stride) + (component * a->component_size);
}