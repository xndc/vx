#pragma once
#include "common.h"
#include <cglm/cglm.h>

typedef struct {
    mat4 proj_matrix;
    mat4 view_matrix;

    float fov;      // horizontal FOV
    float near;
    float far;      // set lower than near for infinite perspective projection

    enum CameraProjection {
        CAMERA_ORTHOGRAPHIC,
        CAMERA_PERSPECTIVE,
    } projection;

    enum CameraMode {
        CAMERA_MODE_NONE,
        CAMERA_MODE_SIMPLE,
        CAMERA_MODE_ORBIT,
        CAMERA_MODE_FPS,
    } mode;

    union CameraModeProps {
        struct CameraModePropsSimple {
            vec3 position;
            vec3 direction; // normalized
        } simple;

        struct CameraModePropsOrbit {
            vec3 target;
            float distance;
            float angle_horz;   // angle relative to world-space Y
            float angle_vert;   // angle relative to camera-space Z after horizontal rotation
        } orbit;

        struct CameraModePropsFPS {
            vec3 position;
            float yaw;      // rotation around world-space Y
            float pitch;    // rotation around camera-space Z after applying yaw
            float roll;     // rotation around camera-space X after applying pitch
            float min_pitch;
            float max_pitch;
            float min_roll;
            float max_roll;
        } fps;
    };

    float prev_fov;
    float prev_near;
    float prev_far;
    enum CameraProjection prev_projection;
    enum CameraMode prev_mode;
    char prev_props [sizeof(union CameraModeProps)];
} Camera;

// Updates the given camera's projection and view matrices.
void UpdateCameraMatrices (Camera* camera, int w, int h);