#pragma once
#include "common.h"
#include "data/camera.h"

extern size_t pfFrameTriCount;
extern size_t pfFrameVertCount;
extern size_t pfFrameDrawCount;

// TODO: move to data/texture.h
extern GLuint rSmpDefault;
extern GLuint rTexWhite1x1;

typedef struct vxConfig {
    int displayW;
    int displayH;
    int shadowSize;
    int envmapSize;
    int skyboxSize;
    Camera camMain;
    Camera camShadow;
    Camera camEnvXp;
    Camera camEnvXn;
    Camera camEnvYp;
    Camera camEnvYn;
    Camera camEnvZp;
    Camera camEnvZn;
} vxConfig;

static void vxConfig_Init (vxConfig* c);