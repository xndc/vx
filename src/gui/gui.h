#pragma once
#include "common.h"
#include "assets.h"

typedef struct GLFWwindow GLFWwindow;
typedef struct ImFont ImFont;
typedef struct vxConfig vxConfig;
typedef struct vxFrame vxFrame;
typedef struct Scene Scene;

#define X(name, path, size) extern ImFont* name;
XM_ASSETS_FONTS
#undef X

VX_EXPORT void GUI_Init (GLFWwindow* window);
VX_EXPORT void GUI_StartFrame();
VX_EXPORT void GUI_Render();
VX_EXPORT void GUI_RenderLoadingFrame (GLFWwindow* window,
    const char* text1, const char* text2,
    float bgr, float bgg, float bgb,
    float fgr, float fgg, float fgb);

VX_EXPORT bool GUI_InterfaceWantsInput();

typedef struct GUI_Statistics {
    size_t frame;
    double msFrame;
    double msMainThread;
    double msRenderThread;
    double msSwapBuffers;
    double msPollEvents;
    size_t drawcalls;
    size_t vertices;
    size_t triangles;
} GUI_Statistics;

VX_EXPORT void GUI_DrawStatistics (vxFrame* frame);

VX_EXPORT void GUI_DrawDebugUI (vxConfig* conf, GLFWwindow* window, Scene* scene);