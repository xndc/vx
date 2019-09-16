#pragma once
#include "common.h"
#include "data/camera.h"

typedef enum TonemapMode {
    TONEMAP_LINEAR,
    TONEMAP_REINHARD,
    TONEMAP_HABLE,
    TONEMAP_ACES,
} TonemapMode;

// For debug visualization, we write values into the aux2 buffer and read them out in the final shader.
// To add a new visualization:
// * Add a new enum to DebugVisMode here
// * Add a shader define for it in sGenerateDefineBlock (program.c)
// * Add a UI option for it in sDrawBufferViewer (gui.cc)
// * In the appropriate shader (gbuf_lighting.frag, usually), write the visualized data out to aux2
// The final shader (final.frag) will read from aux2 if DEBUG_VIS is defined.
typedef enum DebugVisMode {
    DEBUG_VIS_NONE,
    DEBUG_VIS_GBUF_COLOR,
    DEBUG_VIS_GBUF_NORMAL,
    DEBUG_VIS_GBUF_ORM,
    DEBUG_VIS_GBUF_VELOCITY,
    DEBUG_VIS_WORLDPOS,
    DEBUG_VIS_DEPTH_RAW,
    DEBUG_VIS_DEPTH_LINEAR,
    DEBUG_VIS_SHADOWMAP,
} DebugVisMode;

typedef struct vxConfig {
    int swapInterval; // passed to glfwSwapInterval, -1 is translated to 1 on machines without support for it
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
    bool debugShowPointLights;
    bool debugShowLightVolumes;

    float shadowBiasMin;
    float shadowBiasMax;
    int shadowPcfTapsX;
    int shadowPcfTapsY;
    bool shadowHoverFix; // render only backfaces into shadow map
    bool shadowTAA;
    bool shadowNoise;

    // Enable the Temporal Anti-Aliasing filter. Smooths the image at the cost of some blur.
    bool enableTAA;
    // If enabled, use a Halton pattern for the jitter. If disabled, use a simple 2-sample pattern.
    bool taaHaltonJitter;
    // Multiplier for TAA sampling jitter offsets. Original offsets are between [-1,1].
    // Higher multipliers increase both blur and visible jitter on specular surfaces.
    // Going too low results in TAA becoming ineffective (since the sampled positions are almost the same).
    float taaSampleOffsetMul;
    // Offset from current sample to use in neighbourhood clamping. Relevant range is [0,1]. Higher values result in
    // slightly more effective TAA at the cost of extra blur.
    float taaClampSampleDist;
    // Lerp factor for blending between the historical buffer and the current frame. Higher values assign more weight
    // to the historical buffer, resulting in better TAA at the cost of extra blur and more resolve time.
    float taaFeedbackFactor;
    // Strength for the sharpening post-filter. Relevant range is [0, 0.1], with 0.05 being a decent choice.
    float sharpenStrength;
} vxConfig;

void vxConfig_Init (vxConfig* c);

typedef struct vxFrame {
    uint64_t n; // frame number
    float t;   // time at frame start
    float dt;  // delta between current frame start and last frame start
    float tMain;   // time taken by the main processing loop
    float tSubmit; // time taken by draw call submission
    float tRender; // time taken by OpenGL on the GPU side (actual rendering)
    float tSwap;   // time taken by OpenGL on the CPU side (glfwSwapBuffers)
    float tPoll;   // time taken by the operating system (glfwPollEvents)
    uint64_t perfTriangles;
    uint64_t perfVertices;
    uint64_t perfDrawCalls;
    float mouseX;
    float mouseY;
    float mouseDx;
    float mouseDy;
} vxFrame;