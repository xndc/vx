#pragma once
#include "common.h"

typedef struct GLFWwindow GLFWwindow;

#ifdef __cplusplus
    #define EXTERN extern "C"
#else
    #define EXTERN
#endif

EXTERN void uiInit (GLFWwindow* window);
EXTERN void uiDraw (GLFWwindow* window);

#undef EXTERN