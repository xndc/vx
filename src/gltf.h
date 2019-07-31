#pragma once
#include <common.h>
#include <parson/parson.h>

typedef struct Model Model;

static inline JSON_Value* vxReadJSONFile (const char* filename) {
    JSON_Value* root = json_parse_file_with_comments(filename);
    if (root == NULL) {
        FILE* fp = fopen(filename, "r");
        if (fp == NULL) {
            VXPANIC("Failed to read file %s (%s)", filename, strerror(errno));
        } else {
            VXPANIC("Failed to parse JSON from %s (unknown error in parson)", filename);
        }
    }
    return root;
}

// Reads a GLTF scene from disk, returning a Model object.
Model* GLTFSceneRead (const char* directory, const char* filename);