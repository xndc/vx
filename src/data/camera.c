#include "camera.h"

void Camera_InitPerspective (Camera* camera, float zn, float zf, float fov) {
    camera->projection = CAMERA_PERSPECTIVE;
    camera->zn = zn;
    camera->zf = zf;
    camera->fov = fov;
}

void Camera_InitOrtho (Camera* camera, float zoom) {
    camera->projection = CAMERA_ORTHOGRAPHIC;
    camera->zoom = zoom;
}

// Updates the camera, performing the following tasks:
// * Setting its view matrix and aspect ratio
// * Generating a new projection matrix based on its already-set properties
// * Making sure the last and inverse variants of the matrices are correct
void Camera_Update (Camera* camera, int w, int h, mat4 viewMatrix) {
    glm_mat4_copy(camera->proj_matrix, camera->last_proj_matrix);
    glm_mat4_copy(camera->view_matrix, camera->last_view_matrix);
    glm_mat4_copy(viewMatrix, camera->view_matrix);
    glm_mat4_inv(viewMatrix, camera->inv_view_matrix);
    float hw = camera->hw = (float)h / (float)w;
    if (camera->projection != camera->prev_projection || camera->fov != camera->prev_fov ||
        camera->hw != camera->prev_hw || camera->zn != camera->prev_zn || camera->zf != camera->prev_zf)
    {
        if (camera->projection == CAMERA_PERSPECTIVE) {
            // Convert HFOV to VFOV:
            // We assume HFOVs are given for the standard 4:3 aspect ratio rather than the current one. I don't know if
            // this is standard or not, but it matches my intuition of what the FOV slider in a game does.
            float vfov = 2.0f * atanf(tanf((float) vxRadians(camera->fov) / 2.0f) * (3.0f/4.0f));
            // See the following for info about pespective projection matrix generation:
            // * http://dev.theomader.com/depth-precision/ (NOTE: given matrices are transposed)
            // * https://nlguillemot.wordpress.com/2016/12/07/reversed-z-in-opengl/
            // VX also uses reverse depth, for increased precision on hardware that supports it.
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
        } else if (camera->projection == CAMERA_ORTHOGRAPHIC) {
            // Orthographic cameras are weird, probably even more so because we use reverse-Z.
            // I don't fully understand what's going on, but doing the following lets us use
            // "near" and "far" the same way we would for a perspective camera, by specifying
            // plane distances relative to the current position of the camera (as determined by
            // its view matrix). Negative near plane distances can be used to include objects
            // behind the camera in its view.
            float n = -camera->far;
            float f = -camera->near;
            float zw = camera->zoom;
            float zh = camera->zoom * hw;
            glm_ortho(zw/2.0f, zw/-2.0f, zh/-2.0f, zh/2.0f, n, f, camera->proj_matrix);
        }
        glm_mat4_inv(camera->proj_matrix, camera->inv_proj_matrix);
        camera->prev_projection = camera->projection;
        camera->prev_fov = camera->fov;
        camera->prev_zn = camera->zn;
        camera->prev_zf = camera->zf;
        camera->prev_hw = hw;
    }
}

#if 0
void UpdateCameraMatrices (Camera* camera, int w, int h) {
    glm_mat4_copy(camera->proj_matrix, camera->last_proj_matrix);
    glm_mat4_copy(camera->view_matrix, camera->last_view_matrix);
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
                float vfov = 2.0f * atanf(tanf((float) vxRadians(camera->fov) / 2.0f) * hw);
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
                // Orthographic cameras are weird, probably even more so because we use reverse-Z.
                // I don't fully understand what's going on, but doing the following lets us use
                // "near" and "far" the same way we would for a perspective camera, by specifying
                // plane distances relative to the current position of the camera (as determined by
                // its view matrix). Negative near plane distances can be used to include objects
                // behind the camera in its view.
                float n = -camera->far;
                float f = -camera->near;
                float zw = camera->zoom;
                float zh = camera->zoom * hw;
                glm_ortho(zw/2.0f, zw/-2.0f, zh/-2.0f, zh/2.0f, n, f, camera->proj_matrix);
            } break;
        }
        glm_mat4_inv(camera->proj_matrix, camera->inv_proj_matrix);
        camera->prev_aspect_hw = hw;
        camera->prev_fov  = camera->fov;
        camera->prev_near = camera->near;
        camera->prev_far  = camera->far;
        camera->prev_projection = camera->projection;
    }
    // NOTE: We now generate view matrices manually.
    glm_mat4_inv(camera->view_matrix, camera->inv_view_matrix);
}
#endif