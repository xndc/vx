#pragma once
#include "common.h"
#include "data/camera.h"

typedef enum TonemapMode {
    TONEMAP_LINEAR,
    TONEMAP_REINHARD,
    TONEMAP_HABLE,
    TONEMAP_ACES,
} TonemapMode;

typedef enum DebugVisMode {
    DEBUG_VIS_NONE,
    DEBUG_VIS_GBUF_COLOR,
    DEBUG_VIS_GBUF_NORMAL,
    DEBUG_VIS_GBUF_ORM,
    DEBUG_VIS_WORLDPOS,
    DEBUG_VIS_DEPTH_RAW,
    DEBUG_VIS_DEPTH_LINEAR,
} DebugVisMode;

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
    bool pauseOnFocusLoss;
    bool clearColorBuffers;
    bool gpuSupportsClipControl;
    int tonemapMode;
    float tonemapExposure;
    float tonemapACESParamA;
    float tonemapACESParamB;
    float tonemapACESParamC;
    float tonemapACESParamD;
    float tonemapACESParamE;
    int debugVisMode;
} vxConfig;

void vxConfig_Init (vxConfig* c);

typedef struct vxFrame {
    uint64_t n; // frame number
    float t;   // time at frame start
    float dt;  // delta between current frame start and last frame start
    float tMain;   // time taken by the main processing loop
    float tRender; // time taken by the rendering loop
    float tSwap;   // time taken by OpenGL (glfwSwapBuffers)
    float tPoll;   // time taken by the operating system (glfwPollEvents)
    uint64_t perfTriangles;
    uint64_t perfVertices;
    uint64_t perfDrawCalls;
    float mouseX;
    float mouseY;
    float mouseDx;
    float mouseDy;
} vxFrame;