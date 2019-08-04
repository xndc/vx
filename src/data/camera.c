#include "camera.h"

void UpdateCameraMatrices (Camera* camera, int w, int h) {
    float hw = ((float) h / (float) w);
    if (hw != camera->prev_aspect_hw ||
        camera->fov  != camera->prev_fov  ||
        camera->near != camera->prev_near ||
        camera->far  != camera->prev_far  ||
        camera->projection != camera->prev_projection)
    {
        switch (camera->projection) {
            case CAMERA_PERSPECTIVE: {
                // Convert HFOV to VFOV:
                float vfov = 2.0f * atanf(tanf((float) VXRADIANS(camera->fov) / 2.0f) * hw);
                // See the following for info about pespective projection matrix generation:
                // * http://dev.theomader.com/depth-precision/ (NOTE: given matrices are transposed)
                // * https://nlguillemot.wordpress.com/2016/12/07/reversed-z-in-opengl/
                // VX uses reverse depth. We used to support regular depth, but the existence of
                // glDepthRangef as part of GL 4.1 means that's not needed anymore.
                float f = 1.0f / tanf(vfov / 2.0f);
                if (camera->far > camera->near) {
                    // Non-infinite reverse-Z perspective projection:
                    float a = camera->far / (camera->near - camera->far);
                    float b = camera->near * a;
                    glm_mat4_copy((mat4) {
                        f * hw, 0.0f,   0.0f,       0.0f,
                        0.0f,   f,      0.0f,       0.0f,
                        0.0f,   0.0f,   -a-1.0f,    -1.0f,
                        0.0f,   0.0f,   -b,         0.0f
                    }, camera->proj_matrix);
                } else {
                    // Infinite reverse-Z perspective projection:
                    float n = camera->near;
                    glm_mat4_copy((mat4) {
                        f * hw, 0.0f,   0.0f,   0.0f,
                        0.0f,   f,      0.0f,   0.0f,
                        0.0f,   0.0f,   0.0f,   -1.0f,
                        0.0f,   0.0f,   n,      0.0f
                    }, camera->proj_matrix);
                }
            } break;
            case CAMERA_ORTHOGRAPHIC: {
                VXPANIC("TODO");
            } break;
        }
        camera->prev_aspect_hw = hw;
        camera->prev_fov  = camera->fov;
        camera->prev_near = camera->near;
        camera->prev_far  = camera->far;
        camera->prev_projection = camera->projection;
    }
    if (camera->mode != camera->prev_mode ||
        memcmp(&camera->simple, camera->prev_props, sizeof(struct CameraModePropsFPS)) != 0)
    {
        vec3 up = {0.0f, 1.0f, 0.0f};
        switch (camera->mode) {
            case CAMERA_MODE_SIMPLE: {
                struct CameraModePropsSimple p = camera->simple;
                vec3 target = GLM_VEC3_ZERO_INIT;
                glm_vec3_add(p.position, p.direction, target);
                glm_lookat(p.position, target, up, camera->view_matrix);
            } break;
            case CAMERA_MODE_ORBIT: {
                struct CameraModePropsOrbit* p = &camera->orbit;
                vec3 position = GLM_VEC3_ZERO_INIT;
                position[0] = p->target[0] + sinf(p->angle_horz) * p->distance;
                position[1] = p->target[1] + sinf(p->angle_vert) * p->distance;
                position[2] = p->target[2] + cosf(p->angle_horz) * p->distance;
                glm_lookat(position, p->target, up, camera->view_matrix);
            } break;
            case CAMERA_MODE_FPS: {
                VXPANIC("TODO");
            } break;
        }
        camera->prev_mode = camera->mode;
        memcpy(camera->prev_props, &camera->simple, sizeof(struct CameraModePropsFPS));
    }
}