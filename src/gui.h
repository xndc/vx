#pragma once
#include "common.h"
#include "assets.h"

typedef struct GLFWwindow GLFWwindow;
typedef struct ImFont ImFont;

#define X(name, path, size) VXEXPORT ImFont* name;
XM_ASSETS_FONTS
#undef X

VXEXPORT void GUI_Init (GLFWwindow* window);
VXEXPORT void GUI_StartFrame();
VXEXPORT void GUI_Render();
VXEXPORT void GUI_RenderLoadingFrame (GLFWwindow* window,
    const char* text1, const char* text2,
    float bgr, float bgg, float bgb,
    float fgr, float fgg, float fgb);
VXEXPORT void GUI_DrawDebugOverlay (GLFWwindow* window);