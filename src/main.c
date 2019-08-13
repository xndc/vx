#include "common.h"
#include "gui/gui.h"
#include "flib/accessor.h"
#include "data/camera.h"
#include "render/render.h"
#include "scene/scene.h"
#include <glad/glad.h>
#include <glfw/glfw3.h>

static void GlfwErrorCallback (int code, const char* error) {
    vxLog("GLFW error %d: %s", code, error);
}

#ifdef GLAD_DEBUG
static void GladPostCallback (const char *name, void *funcptr, int len_args, ...) {
    GLenum error;
    // NOTE: glGetError seems to crash the Nvidia Windows driver for some reason, under some
    //   circumstances that I can't figure out. If you get weird crashes in an nvoglv64 worker
    //   thread, remove this call and see if they go away.
    error = glad_glGetError();
    if (error != GL_NO_ERROR) {
        const char* str = "<UNKNOWN>";
        switch (error) {
            // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glGetError.xhtml
            case GL_INVALID_ENUM:                  { str = "GL_INVALID_ENUM";                  } break;
            case GL_INVALID_VALUE:                 { str = "GL_INVALID_VALUE";                 } break;
            case GL_INVALID_OPERATION:             { str = "GL_INVALID_OPERATION";             } break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: { str = "GL_INVALID_FRAMEBUFFER_OPERATION"; } break;
            case GL_STACK_OVERFLOW:                { str = "GL_STACK_OVERFLOW";                } break;
            case GL_STACK_UNDERFLOW:               { str = "GL_STACK_UNDERFLOW";               } break;
            case GL_OUT_OF_MEMORY:                 { str = "GL_OUT_OF_MEMORY";                 } break;
        }
        vxLog("%s (%d) in %s", str, error, name);
    }
}
#endif

int G_WindowConfig_W = 1280;
int G_WindowConfig_H = 1024;
int G_RenderConfig_ShadowMapSize = 2048;
Camera G_MainCamera = {0};
Camera G_ShadowCamera = {0};

int main() {
    vxEnableSignalHandlers();
    vxConfigureLogging();
    glfwSetErrorCallback(GlfwErrorCallback);
    vxCheck(glfwInit());

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, true);
    GLFWwindow* window = glfwCreateWindow(G_WindowConfig_W, G_WindowConfig_H, "VX", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGL();
    #ifdef GLAD_DEBUG
    glad_set_post_callback(GladPostCallback);
    #endif

    // Retrieve the correct glTextureBarrier (regular or NV) function for this system
    if (glfwExtensionSupported("GL_NV_texture_barrier")) {
        vxglTextureBarrier = (PFNGLTEXTUREBARRIERPROC) glTextureBarrierNV;
    } else if (glfwExtensionSupported("GL_ARB_texture_barrier")) {
        vxglTextureBarrier = glTextureBarrier;
    } else {
        vxPanic("This program requires the GL_*_texture_barrier extension.");
    }
    vxAssert(GL_COLOR_ATTACHMENT0 == VXGL_COLOR_ATTACHMENT0);

    // Reverse Z setup:
    glDepthRangef(-1.0f, 1.0f);

    // Load assets and display loading screen:
    glfwSwapInterval(0);
    GUI_Init(window);
    #define LOADING_BG 0.2f, 0.3f, 0.4f
    #define LOADING_FG 1.0f, 1.0f, 1.0f
    GUI_RenderLoadingFrame(window, "Loading assets...", "", LOADING_BG, LOADING_FG);
    // Shaders:
    #define X(name, path) name = LoadShaderFromDisk(#name, path);
    XM_ASSETS_SHADERS
    #undef X
    // Models:
    #define X(name, dir, file) ReadModelFromDisk(#name, &name, dir, file);
    XM_ASSETS_MODELS_GLTF
    #undef X
    // Textures:
    #define X(name, path) name = LoadTextureFromDisk(#name, path);
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

    MDL_DUCK.materials[0].stipple = true;
    MDL_DUCK.materials[0].stipple_hard_cutoff = 0.0f;
    MDL_DUCK.materials[0].stipple_soft_cutoff = 1.0f;
    MDL_DUCK.materials[0].const_diffuse[0] = 1.0f;
    MDL_DUCK.materials[0].const_diffuse[1] = 1.0f;
    MDL_DUCK.materials[0].const_diffuse[2] = 1.0f;
    MDL_DUCK.materials[0].const_diffuse[3] = 0.5f;

    while (!glfwWindowShouldClose(window)) {
        static double lastFrameFullTime;
        static double lastFrameMainTime;
        static double lastFrameRenderTime;
        static double tFrameStart;
        static double tFrameRenderStart;
        static double tFramePreSync;
        static double tFramePostSync;

        // Pause the game if it loses focus:
        if (!glfwGetWindowAttrib(window, GLFW_FOCUSED)) {
            double t = glfwGetTime();
            glfwWaitEvents();
            glfwSetTime(t);
            continue;
        }

        tFrameStart = glfwGetTime();
        vxAdvanceFrame();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        UpdateFramebuffers(w, h, G_RenderConfig_ShadowMapSize);
        glViewport(0, 0, w, h);

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

        tFrameRenderStart = glfwGetTime();

        StartRenderPass("Framebuffer clear");
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FB_MAIN);
        glDrawBuffers(FB_MAIN_BUFFER_COUNT, FB_MAIN_BUFFERS);
        glClearColor(0.3f, 0.4f, 0.5f, 1.0f);
        glClearDepth(0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        StartRenderPass("Main Render");
        SetCameraMatrices(&G_MainCamera);
        SetRenderProgram(VSH_DEFAULT, FSH_FWD_OPAQUE);

        ResetModelMatrix();
        AddModelPosition((vec3){-0.1f, -0.5f, 0.0f});
        RenderModel(&MDL_DUCK, w, h);

        ResetModelMatrix();
        AddModelPosition((vec3){0.0f, -1.0f, 0.0f});
        AddModelScale((vec3){1.2f, 1.2f, 1.2f});
        RenderModel(&MDL_BOX_MR, w, h);

        ResetModelMatrix();
        AddModelPosition((vec3){0.0f, -4.0f, 0.0f});
        AddModelScale((vec3){2.5f, 2.5f, 2.5f});
        RenderModel(&MDL_SPONZA, w, h);

        // StartRenderPass("Dither Effect");
        // glDrawBuffers(FB_MAIN_BUFFER_COUNT, FB_MAIN_BUFFERS);
        // SetRenderProgram(VSH_FULLSCREEN_PASS, FSH_FX_DITHER);
        // RunFullscreenPass(w, h);

        StartRenderPass("Final Pass");
        SetRenderProgram(VSH_FULLSCREEN_PASS, FSH_FINAL);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);
        RunFullscreenPass(w, h);

        StartRenderPass("GUI");
        GUI_StartFrame();
        static bool showDebugOverlay = false;
        static int debugOverlayKeyState = GLFW_RELEASE;
        if (glfwGetKey(window, GLFW_KEY_GRAVE_ACCENT) == GLFW_PRESS && debugOverlayKeyState == GLFW_RELEASE) {
            showDebugOverlay = !showDebugOverlay;
        }
        debugOverlayKeyState = glfwGetKey(window, GLFW_KEY_GRAVE_ACCENT);
        if (showDebugOverlay) {
            static GUI_Statistics stats;
            stats.msFrame        = lastFrameFullTime   * 1000.0f;
            stats.msMainThread   = lastFrameMainTime   * 1000.0f;
            stats.msRenderThread = lastFrameRenderTime * 1000.0f;
            stats.drawcalls = 0;
            stats.triangles = 0;
            stats.vertices  = 0;
            GUI_DrawStatistics(&stats);
            GUI_DrawDebugOverlay(window);
        }
        GUI_Render();

        tFramePreSync = glfwGetTime();
        glfwSwapBuffers(window);
        glfwPollEvents();
        tFramePostSync = glfwGetTime();

        lastFrameFullTime = tFramePostSync - tFrameStart;
        lastFrameMainTime = tFrameRenderStart - tFrameStart;
        lastFrameRenderTime = tFramePreSync - tFrameRenderStart;
    }

    return 0;
}
