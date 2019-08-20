#pragma once
#include "common.h"
#include "assets.h"

typedef struct GLFWwindow GLFWwindow;
typedef struct ImFont ImFont;

#define X(name, path, size) extern ImFont* name;
XM_ASSETS_FONTS
#undef X

void GUI_Init (GLFWwindow* window);
void GUI_StartFrame();
void GUI_Render();
void GUI_RenderLoadingFrame (GLFWwindow* window,
    const char* text1, const char* text2,
    float bgr, float bgg, float bgb,
    float fgr, float fgg, float fgb);
void GUI_DrawDebugOverlay (GLFWwindow* window);

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

void GUI_DrawStatistics (GUI_Statistics* stats);