#pragma once
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
void GUI_DrawLoadingText (const char* text, float r, float g, float b);
void GUI_DrawDebugOverlay (GLFWwindow* window);