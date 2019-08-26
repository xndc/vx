#pragma once
#include "common.h"

typedef struct {
    mat4 proj_matrix;
    mat4 view_matrix;
    mat4 inv_proj_matrix;
    mat4 inv_view_matrix;
    mat4 last_proj_matrix;
    mat4 last_view_matrix;

    enum CameraProjection {
        CAMERA_ORTHOGRAPHIC,
        CAMERA_PERSPECTIVE,
    } projection;

    // TODO: switch from near/far to zn/zf across the entire program (for windows.h/unity build compatibility)
    union { float zn; float near; };
    union { float zf; float far;  }; // set lower than near for infinite perspective projection
    float fov;  // horizontal FOV at aspect ratio 4:3, in perspective mode
    float zoom; // zoom factor (world units per [0,1] clip space unit) in orthographic mode

    float hw; // aspect ratio

    enum CameraProjection prev_projection;
    float prev_fov;
    float prev_zn;
    float prev_zf;
    float prev_hw;
} Camera;

void Camera_InitPerspective (Camera* camera, float zn, float zf, float fov);
void Camera_InitOrtho (Camera* camera, float zoom);
void Camera_Update (Camera* camera, int w, int h, mat4 viewMatrix);