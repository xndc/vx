#include "main.h"
#include "gui/gui.h"
#include "flib/accessor.h"
#include "data/camera.h"
#include "render/render.h"
#include "scene/scene.h"
#include <glad/glad.h>
#include <glfw/glfw3.h>

size_t pfFrameTriCount = 0;
size_t pfFrameVertCount = 0;
size_t pfFrameDrawCount = 0;

GLuint rSmpDefault = 0;
GLuint rTexWhite1x1 = 0;

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
    glfwWindowHint(GLFW_SRGB_CAPABLE, true);
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

    // Reverse Z setup:
    glDepthRangef(-1.0f, 1.0f);

    // Load default texture and sampler: (1x1 white square)
    glGenTextures(1, &rTexWhite1x1);
    glGenSamplers(1, &rSmpDefault);
    glSamplerParameteri(rSmpDefault, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glSamplerParameteri(rSmpDefault, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glSamplerParameteri(rSmpDefault, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(rSmpDefault, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, rTexWhite1x1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
        (GLubyte[]){ 255, 255, 255, 255 });

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

    // HACK: Quick and dirty 30 FPS lock for the MacBook.
    // Locking to 60FPS or not locking at all results in HEAVY stuttering.
    #ifdef __APPLE__
    glfwSwapInterval(2);
    #endif

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

    // MDL_DUCK.materials[0].stipple = true;
    // MDL_DUCK.materials[0].stipple_hard_cutoff = 0.0f;
    // MDL_DUCK.materials[0].stipple_soft_cutoff = 1.0f;

    while (!glfwWindowShouldClose(window)) {
        static double lastFrameFullTime;
        static double lastFrameMainTime;
        static double lastFrameRenderTime;
        static double lastFrameSwapBuffersTime;
        static double lastFramePollEventsTime;
        static double t; // frame start
        static double tFrameRenderStart;
        static double tFramePreSync;
        static double tFramePostSync;
        static double tFramePostSwap;
        t = glfwGetTime();

        // Pause the game if it loses focus:
        if (!glfwGetWindowAttrib(window, GLFW_FOCUSED)) {
            glfwWaitEvents();
            glfwSetTime(t);
            continue;
        }

        vxAdvanceFrame();
        pfFrameTriCount = 0;
        pfFrameVertCount = 0;
        pfFrameDrawCount = 0;

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        UpdateFramebuffers(w, h, G_RenderConfig_ShadowMapSize);
        glViewport(0, 0, w, h);

        const float dist = 3.0f;
        float angle_h = glfwGetTime() * 0.5f;
        float angle_v = (M_PI * 0.5f) + cosf(t * 0.5f) * 0.35f;
        // NOTE: this is a standard spherical-to-cartesian coordinate mapping with
        //   radius/rho = dist, theta = angle_v, phi = angle_h, and the Y/Z axes swapped
        G_MainCamera.position[0] = dist * sinf(angle_v) * cosf(angle_h);
        G_MainCamera.position[1] = dist * cosf(angle_v);
        G_MainCamera.position[2] = dist * sinf(angle_v) * sinf(angle_h);
        glm_vec3_add(G_MainCamera.position, G_MainCamera.target, G_MainCamera.position);
        UpdateCameraMatrices(&G_MainCamera, w, h);

        MDL_DUCK.materials[0].const_diffuse[3] = (sin(3.0 * t) + 1.0) * 0.51;

        tFrameRenderStart = glfwGetTime();

        StartRenderPass("GBuffer clear");
        BindFramebuffer(FB_GBUFFER);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearDepth(0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        StartRenderPass("GBuffer render");
        BindFramebuffer(FB_GBUFFER);
        SetCameraMatrices(&G_MainCamera);
        SetRenderProgram(VSH_DEFAULT, FSH_GBUF_MAIN);

        ResetModelMatrix();
        AddModelPosition((vec3){-0.1f, -0.5f, 0.0f});
        RenderModel(&MDL_DUCK, w, h, t, vxFrameNumber);

        ResetModelMatrix();
        AddModelPosition((vec3){0.0f, -1.0f, 0.0f});
        AddModelScale((vec3){1.2f, 1.2f, 1.2f});
        RenderModel(&MDL_BOX_MR, w, h, t, vxFrameNumber);

        ResetModelMatrix();
        AddModelPosition((vec3){0.0f, -4.0f, 0.0f});
        AddModelScale((vec3){2.5f, 2.5f, 2.5f});
        RenderModel(&MDL_SPONZA, w, h, t, vxFrameNumber);

        StartRenderPass("GBuffer lighting");
        BindFramebuffer(FB_ONLY_COLOR_HDR);
        SetRenderProgram(VSH_FULLSCREEN_PASS, FSH_GBUF_LIGHTING);
        SetCameraMatrices(&G_MainCamera);
        // Ambient lighting:
        float i = 0.2f;
        glUniform3fv(UNIF_AMBIENT_CUBE, 6, (float[]){
            0.8f * i, 0.8f * i, 0.8f * i,  // Y+ (sky)
            0.3f * i, 0.3f * i, 0.3f * i,  // Y- (ground)
            0.6f * i, 0.6f * i, 0.6f * i,  // Z+ (north)
            0.4f * i, 0.4f * i, 0.4f * i,  // Z- (south)
            0.6f * i, 0.6f * i, 0.6f * i,  // X+ (east)
            0.4f * i, 0.4f * i, 0.4f * i,  // X- (west)
        });
        // Directional lighting:
        glUniform3f(UNIF_SUN_DIRECTION, -1.0f, -1.0f, -1.0f);
        glUniform3f(UNIF_SUN_COLOR, 1.0f, 1.0f, 1.0f);
        // Point lighting:
        glUniform3fv(UNIF_POINTLIGHT_POSITIONS, 4, (float[]){
             2.9f,  2.0f,  2.2f,
            -2.8f,  2.5f,  2.4f,
            -2.2f,  2.3f, -2.6f,
             2.1f,  2.9f, -2.5f,
        });
        glUniform3fv(UNIF_POINTLIGHT_COLORS, 4, (float[]){
            40.0f, 40.0f, 40.0f,
            40.0f, 40.0f, 40.0f,
            40.0f, 40.0f, 40.0f,
            40.0f, 40.0f, 40.0f,
        });
        // Run pass:
        RunFullscreenPass(w, h, t, vxFrameNumber);

        // StartRenderPass("Dither Effect");
        // BindFramebuffer(FB_ONLY_COLOR_LDR);
        // SetRenderProgram(VSH_FULLSCREEN_PASS, FSH_FX_DITHER);
        // RunFullscreenPass(w, h, t, vxFrameNumber);

        StartRenderPass("Final Pass");
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        SetRenderProgram(VSH_FULLSCREEN_PASS, FSH_FINAL);
        SetCameraMatrices(&G_MainCamera);
        glDrawBuffer(GL_BACK);
        RunFullscreenPass(w, h, t, vxFrameNumber);

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
            stats.frame = vxFrameNumber;
            stats.msFrame        = lastFrameFullTime        * 1000.0f;
            stats.msMainThread   = lastFrameMainTime        * 1000.0f;
            stats.msRenderThread = lastFrameRenderTime      * 1000.0f;
            stats.msSwapBuffers  = lastFrameSwapBuffersTime * 1000.0f;
            stats.msPollEvents   = lastFramePollEventsTime  * 1000.0f;
            stats.drawcalls = pfFrameDrawCount;
            stats.triangles = pfFrameTriCount;
            stats.vertices  = pfFrameVertCount;
            GUI_DrawStatistics(&stats);
            GUI_DrawDebugOverlay(window);
        }
        GUI_Render();

        tFramePreSync = glfwGetTime();
        glfwSwapBuffers(window);
        tFramePostSwap = glfwGetTime();
        glfwPollEvents();
        tFramePostSync = glfwGetTime();

        lastFrameFullTime = tFramePostSync - t;
        lastFrameMainTime = tFrameRenderStart - t;
        lastFrameRenderTime = tFramePreSync - tFrameRenderStart;
        lastFrameSwapBuffersTime = tFramePostSwap - tFramePreSync;
        lastFramePollEventsTime = tFramePostSync - tFramePostSwap;

        // PollEvents usually takes under 1ms, so if we exceed 100ms it's probably because the
        // window is moving. Ignore the frame for timing purposes if this happens.
        if (lastFramePollEventsTime > 0.1f) {
            lastFramePollEventsTime = 0.1f;
            lastFrameFullTime = tFramePostSwap - t;
            glfwSetTime(tFramePostSwap);
        }
    }

    return 0;
}
