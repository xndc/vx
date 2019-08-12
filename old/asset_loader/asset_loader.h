#pragma once
#include "common.h"
#include "assets.h"
#include "data/model.h"
#include "data/texture.h"

GLuint QueueTextureLoad (char* path, GLenum target, bool mipmaps);
GLuint QueueShaderLoad (char* vshPath, char* fshPath);
Model* QueueModelLoad (char* directory, char* filename);

void QueueDefaultAssets();
void LoadNextQueuedAsset();