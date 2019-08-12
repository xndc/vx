#pragma once
#include "common.h"
#include "assets.h"

typedef struct GLFWwindow GLFWwindow;
typedef struct ImFont ImFont;

#define X(name, path, size) VX_EXPORT ImFont* name;
XM_ASSETS_FONTS
#undef X

VX_EXPORT void GUI_Init (GLFWwindow* window);
VX_EXPORT void GUI_StartFrame();
VX_EXPORT void GUI_Render();
VX_EXPORT void GUI_RenderLoadingFrame (GLFWwindow* window,
    const char* text1, const char* text2,
    float bgr, float bgg, float bgb,
    float fgr, float fgg, float fgb);
VX_EXPORT void GUI_DrawDebugOverlay (GLFWwindow* window);