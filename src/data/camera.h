#pragma once
#include "common.h"

typedef struct {
    mat4 proj_matrix;
    mat4 view_matrix;

    enum CameraProjection {
        CAMERA_ORTHOGRAPHIC,
        CAMERA_PERSPECTIVE,
    } projection;

    float near;
    float far;      // set lower than near for infinite perspective projection
    float fov;      // horizontal FOV in perspective mode
    float zoom;     // zoom factor (world units per [0,1] clip space unit) in orthographic mode

    vec3 position;
    vec3 target;

    float prev_fov;
    float prev_near;
    float prev_far;
    float prev_aspect_hw;
    enum CameraProjection prev_projection;
    vec3 prev_position;
    vec3 prev_target;
} Camera;

// Updates the given camera's projection and view matrices.
void UpdateCameraMatrices (Camera* camera, int w, int h);