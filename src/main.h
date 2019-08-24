#pragma once
#include "common.h"
#include "data/camera.h"

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