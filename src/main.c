#include "common.h"
#include "gui.h"
#include "flib/accessor.h"
#include "flib/array.h"
#include "data/camera.h"
#include "render/render.h"
#include <glad/glad.h>
#include <glfw/glfw3.h>

static void GlfwErrorCallback (int code, const char* error) {
    VXERROR("GLFW error %d: %s", code, error);
}

int G_WindowConfig_W = 1280;
int G_WindowConfig_H = 1024;
int G_RenderConfig_ShadowMapSize = 2048;
Camera G_MainCamera = {0};
Camera G_ShadowCamera = {0};

int main() {
    glfwSetErrorCallback(GlfwErrorCallback);
    VXCHECK(glfwInit());

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, true);
    GLFWwindow* window = glfwCreateWindow(G_WindowConfig_W, G_WindowConfig_H, "VX", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGL();

    // Reverse Z setup:
    glDepthRangef(-1.0f, 1.0f);

    // Load assets and display loading screen:
    glfwSwapInterval(0);
    GUI_Init(window);
    #define LOADING_BG 0.2f, 0.3f, 0.4f
    #define LOADING_FG 1.0f, 1.0f, 1.0f
    // Shaders:
    #define X(name, path) \
        GUI_RenderLoadingFrame(window, "Loading shaders", path, LOADING_BG, LOADING_FG); \
        name = LoadShaderFromDisk(#name, path);
    XM_ASSETS_SHADERS
    #undef X
    // Models:
    #define X(name, dir, file) \
        GUI_RenderLoadingFrame(window, "Loading models", dir "/" file, LOADING_BG, LOADING_FG); \
        ReadModelFromDisk(#name, &name, dir, file);
    XM_ASSETS_MODELS_GLTF
    #undef X
    // Textures:
    #define X(name, path) \
        GUI_RenderLoadingFrame(window, "Loading textures", path, LOADING_BG, LOADING_FG); \
        name = LoadTextureFromDisk(#name, path);
    XM_ASSETS_TEXTURES
    #undef X
    // Samplers:
    glGenSamplers(VXGL_SAMPLER_COUNT, VXGL_SAMPLER);
    #undef LOADING_BG
    #undef LOADING_FG

    if (glfwExtensionSupported("WGL_EXT_swap_control_tear") ||
        glfwExtensionSupported("GLX_EXT_swap_control_tear")) {
        glfwSwapInterval(-1);
    } else {
        glfwSwapInterval(1);
    }

    if (!glfwExtensionSupported("GL_NV_texture_barrier")) {
        VXPANIC("This program requires the GL_NV_texture_barrier extension.");
    }

    #if 1
    G_MainCamera.projection = CAMERA_PERSPECTIVE;
    G_MainCamera.fov  = 80.0f;
    G_MainCamera.near = 0.1f;
    G_MainCamera.far  = 0.0f;
    #else
    G_MainCamera.projection = CAMERA_ORTHOGRAPHIC;
    G_MainCamera.near = -100.0f;
    G_MainCamera.far  = +100.0f;
    G_MainCamera.zoom = 40.0f;
    #endif
    glm_vec3_copy((vec3){0.0f, 0.5f, 0.0f}, G_MainCamera.target);

    while (!glfwWindowShouldClose(window)) {
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        UpdateFramebuffers(w, h, G_RenderConfig_ShadowMapSize);
        glViewport(0, 0, w, h);
        GUI_StartFrame();

        const float dist = 3.0f;
        float angle_h = glfwGetTime() * 0.5f;
        float angle_v = (M_PI * 0.5f) + cosf(glfwGetTime() * 0.5f) * 0.35f;
        // NOTE: this is a standard spherical-to-cartesian coordinate mapping with
        //   radius/rho = dist, theta = angle_v, phi = angle_h, and the Y/Z axes swapped
        G_MainCamera.position[0] = dist * sinf(angle_v) * cosf(angle_h);
        G_MainCamera.position[1] = dist * cosf(angle_v);
        G_MainCamera.position[2] = dist * sinf(angle_v) * sinf(angle_h);
        glm_vec3_add(G_MainCamera.position, G_MainCamera.target, G_MainCamera.position);
        UpdateCameraMatrices(&G_MainCamera, w, h);

        StartRenderPass("Framebuffer clear");
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FB_MAIN);
        static const GLenum drawbuffers[] = {
            GL_COLOR_ATTACHMENT0,
            GL_COLOR_ATTACHMENT1,
            GL_COLOR_ATTACHMENT2,
            GL_COLOR_ATTACHMENT3,
            GL_COLOR_ATTACHMENT4,
            GL_COLOR_ATTACHMENT5,
            GL_COLOR_ATTACHMENT6,
        };
        glDrawBuffers(VXSIZE(drawbuffers), drawbuffers);
        glClearColor(0.3f, 0.4f, 0.5f, 1.0f);
        glClearDepth(0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        StartRenderPass("Test");
        SetCameraMatrices(&G_MainCamera);
        SetRenderProgram(VSH_DEFAULT, FSH_DEFAULT);

        ResetModelMatrix();
        AddModelPosition((vec3){-0.1f, -0.5f, 0.0f});
        RenderModel(&MDL_DUCK);

        ResetModelMatrix();
        AddModelPosition((vec3){0.0f, -1.0f, 0.0f});
        AddModelScale((vec3){1.2f, 1.2f, 1.2f});
        RenderModel(&MDL_BOX_MR);

        ResetModelMatrix();
        AddModelPosition((vec3){0.0f, -4.0f, 0.0f});
        AddModelScale((vec3){2.5f, 2.5f, 2.5f});
        RenderModel(&MDL_SPONZA);

        StartRenderPass("Fullscreen Pass Test");
        glDrawBuffers(VXSIZE(drawbuffers), drawbuffers);
        SetRenderProgram(VSH_FULLSCREEN_PASS, FSH_FX_DITHER);
        RunFullscreenPass(w, h);

        StartRenderPass("Final Pass");
        SetRenderProgram(VSH_FULLSCREEN_PASS, FSH_FINAL);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);
        RunFullscreenPass(w, h);

        StartRenderPass("GUI");
        GUI_DrawDebugOverlay(window);
        GUI_Render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    return 0;
}