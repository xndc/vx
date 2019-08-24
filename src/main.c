#include "main.h"
#include "gui/gui.h"
#include "flib/accessor.h"
#include "data/camera.h"
#include "data/texture.h"
#include "render/render.h"
#include "render/program.h"
#include <glad/glad.h>
#include <glfw/glfw3.h>

size_t pfFrameTriCount = 0;
size_t pfFrameVertCount = 0;
size_t pfFrameDrawCount = 0;

GLuint rSmpDefault = 0;
GLuint rTexWhite1x1 = 0;

static void sGlfwErrorCallback (int code, const char* error) {
    vxLog("GLFW error %d: %s", code, error);
}

#ifdef GLAD_DEBUG
static void sGladPostCallback (const char *name, void *funcptr, int len_args, ...) {
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

void vxConfig_Init (vxConfig* c) {
    c->displayW = 1280;
    c->displayH = 1024;
    c->shadowSize = 2048;
    c->envmapSize = 512;
    c->skyboxSize = 2048;
    Camera_InitPerspective(&c->camMain, 0.01f, 0.0f, 80.0f);
    glm_lookat((vec3){3.0f, 3.0f, 3.0f}, GLM_VEC3_ZERO, VX_UP, c->camMain.view_matrix);
    Camera_InitOrtho(&c->camShadow, 100.0f);
    glm_lookat((vec3){-500.0f, 700.0f, -1000.0f}, GLM_VEC3_ZERO, VX_UP, c->camShadow.view_matrix);
    Camera_InitPerspective(&c->camEnvXp, 0.1f, 0.0f, 90.0f);
    Camera_InitPerspective(&c->camEnvXn, 0.1f, 0.0f, 90.0f);
    Camera_InitPerspective(&c->camEnvYp, 0.1f, 0.0f, 90.0f);
    Camera_InitPerspective(&c->camEnvYn, 0.1f, 0.0f, 90.0f);
    Camera_InitPerspective(&c->camEnvZp, 0.1f, 0.0f, 90.0f);
    Camera_InitPerspective(&c->camEnvZn, 0.1f, 0.0f, 90.0f);
    glm_lookat(GLM_VEC3_ZERO, (vec3){+1.0f, +0.0f, +0.0f}, VX_UP, c->camEnvXp.view_matrix);
    glm_lookat(GLM_VEC3_ZERO, (vec3){-1.0f, -0.0f, -0.0f}, VX_UP, c->camEnvXn.view_matrix);
    glm_lookat(GLM_VEC3_ZERO, (vec3){+0.0f, +1.0f, +0.0f}, VX_UP, c->camEnvYp.view_matrix);
    glm_lookat(GLM_VEC3_ZERO, (vec3){-0.0f, -1.0f, -0.0f}, VX_UP, c->camEnvYn.view_matrix);
    glm_lookat(GLM_VEC3_ZERO, (vec3){+0.0f, +0.0f, +1.0f}, VX_UP, c->camEnvZp.view_matrix);
    glm_lookat(GLM_VEC3_ZERO, (vec3){-0.0f, -0.0f, -1.0f}, VX_UP, c->camEnvZn.view_matrix);
    c->pauseOnFocusLoss = false;
}

// Initializes the game. Should only be run once, at the start of its execution.
void GameLoad (vxConfig* conf, GLFWwindow** pwindow) {
    vxEnableSignalHandlers();
    vxConfigureLogging();
    vxConfig_Init(conf);

    // Initialize GLFW:
    glfwSetErrorCallback(sGlfwErrorCallback);
    vxCheck(glfwInit());

    // Initialize the game window and OpenGL context:
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, true);
    glfwWindowHint(GLFW_SRGB_CAPABLE, true);
    GLFWwindow* window = glfwCreateWindow(conf->displayW, conf->displayH, "VX", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGL();
    #ifdef GLAD_DEBUG
    glad_set_post_callback(sGladPostCallback);
    #endif

    // Reverse Z setup:
    if (glfwExtensionSupported("GL_ARB_clip_control")) {
        conf->gpuSupportsClipControl = true;
        glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
    } else if (glfwExtensionSupported("NV_depth_buffer_float")) {
        conf->gpuSupportsClipControl = true;
        glDepthRangedNV(-1.0f, 1.0f);
    } else {
        conf->gpuSupportsClipControl = false;
        vxLog("Warning: This machine's graphics driver doesn't support reverse-Z mapping.");
        // Without modifying OpenGL's clip behaviour, our shaders and transform matrices will map depth to [1, 0.5]
        // rather than [1, 0]. This has two main caveats:
        // * Bad precision. The whole point of reverse Z is to exploit the increased precision around z=0.
        // * Depth has to be transformed (z = z*2-1) in every fragment shader.
        // glDepthRangef is not a suitable alternative to glDepthRangedNV, despite some claims that it is. While the
        // OpenGL standard technically allows it to work with unclamped values (i.e. -1.0f), both the Nvidia and Apple
        // drivers I've tested this on will actually clamp values to [0,1].
    }

    // VSync setup:
    if (glfwExtensionSupported("WGL_EXT_swap_control_tear") ||
        glfwExtensionSupported("GLX_EXT_swap_control_tear")) {
        glfwSwapInterval(-1);
    } else {
        glfwSwapInterval(1);
    }

    // Window configuration:
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    // HACK: 30 FPS lock for the MacBook
    // Locking to 60FPS or not at all results in heavy stuttering. It seems like the macOS compositor expects one frame
    // per VSync interval and just drops everything else. TODO: Try out triple buffering as an alternative solution.
    #ifdef __APPLE__
    glfwSwapInterval(2);
    #endif

    // Initialize game subsystems:
    InitTextureSystem();
    InitProgramSystem(conf);
    InitRenderSystem();
    GUI_Init(window);

    *pwindow = window;
}

// Reloads the game's assets. Can be run multiple times.
void GameReload (vxConfig* conf, GLFWwindow* window) {
    GUI_RenderLoadingFrame(window, "Loading...", "", 0.2f, 0.3f, 0.4f, 0.9f, 0.9f, 0.9f);
    LoadTextures();
    LoadModels();
}

// Tick function for the game. Generates a single frame.
void GameTick (vxConfig* conf, GLFWwindow* window, vxFrame* frame, vxFrame* lastFrame) {
    frame->t = glfwGetTime();
    frame->dt = frame->t - lastFrame->t;

    // Pause the game if it loses focus:
    if (conf->pauseOnFocusLoss && !glfwGetWindowAttrib(window, GLFW_FOCUSED)) {
        glfwWaitEvents();
        glfwSetTime(frame->t);
        return;
    }
    vxAdvanceFrame();

    // *****************************************************************************************************************
    // Update:

    // Retrieve new framebuffer size and update framebuffers if required:
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    conf->displayW = w;
    conf->displayH = h;
    uint8_t updatedTargets = UpdateRenderTargets(conf);
    glViewport(0, 0, w, h);
    
    // Run subsystem tick functions:
    UpdatePrograms(conf);
    GUI_StartFrame();

    // Manage cursor lock status:
    // TODO: don't lock when clicking on ImGui elements
    // TODO: don't affect ImGui elements when locked
    bool cursorLocked = (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED);
    if (!cursorLocked && (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        cursorLocked = true;
    }
    if (cursorLocked && glfwGetKey(window, GLFW_KEY_ESCAPE)) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        cursorLocked = false;
    }

    // FPS-style camera movement & mouse-look:
    double mx, my, dmx, dmy;
    {
        static float lmx = 0;
        static float lmy = 0;
        static float ry = 0;
        static float rx = 0;
        glfwGetCursorPos(window, &mx, &my);
        dmx = lmx - (float) mx;
        dmy = lmy - (float) my;
        if (cursorLocked) {
            const float sx = -0.002f;
            const float sy = -0.002f;
            ry += dmx * sx;
            rx += dmy * sy;
            rx = vxClamp(rx, vxRadians(-90.0f), vxRadians(90.0f));
            if (ry < vxRadians(-360)) ry += vxRadians(360);
            if (ry > vxRadians(+360)) ry -= vxRadians(360);
        }
        lmx = (float) mx;
        lmy = (float) my;

        vec4 q  = GLM_QUAT_IDENTITY_INIT;
        vec4 qy = GLM_QUAT_IDENTITY_INIT;
        vec4 qx = GLM_QUAT_IDENTITY_INIT;
        glm_quat(qy, ry, 0, 1, 0);
        glm_quat(qx, rx, 1, 0, 0);
        glm_quat_mul(qx, qy, q);

        static vec3 pos = {-2, -2, -2};
        // FIXME: I have no idea why these have to be negative.
        vec3 spd = {-7.0f * frame->dt, -7.0f * frame->dt, -7.0f * frame->dt};
        vec3 dpos = GLM_VEC3_ZERO_INIT;
        if (glfwGetKey(window, GLFW_KEY_SPACE)      == GLFW_PRESS) { pos[1] += spd[1]; }
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) { pos[1] -= spd[1]; }
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { dpos[2] = -1; }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { dpos[2] = +1; }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { dpos[0] = -1; }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { dpos[0] = +1; }
        glm_vec3_mul(dpos, spd, dpos);
        glm_vec3_rotate(pos, ry, (vec3){0, 1, 0});
        glm_vec3_add(pos, dpos, pos);
        glm_vec3_rotate(pos, -ry, (vec3){0, 1, 0});

        mat4 vmat;
        glm_quat_mat4(q, vmat);
        glm_translate(vmat, pos);
        Camera_Update(&conf->camMain, w, h, vmat);
    }
    frame->mouseX = (float) mx;
    frame->mouseY = (float) my;
    frame->mouseDx = (float) dmx;
    frame->mouseDy = (float) dmy;

    // Debug user interface:
    GUI_DrawStatistics(lastFrame);
    
    // *****************************************************************************************************************
    // Render:

    double tRenderStart = glfwGetTime();
    static RenderState rs;
    glViewport(0, 0, conf->displayW, conf->displayH);

    if (updatedTargets & UPDATED_ENVMAP_TARGETS) {
        // TODO: generate environment maps
    }
    
    if (conf->clearColorBuffers) {
        StartRenderPass(&rs, "GBuffer clear (color+depth)");
        BindFramebuffer(FB_GBUFFER);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearDepth(0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    } else {
        StartRenderPass(&rs, "GBuffer clear (depth-only)");
        BindFramebuffer(FB_GBUFFER);
        glClearDepth(0.0f);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    StartRenderPass(&rs, "GBuffer main (opaque objects)");
    BindFramebuffer(FB_GBUFFER);
    SetRenderProgram(&rs, &PROG_GBUF_MAIN);
    SetCamera(&rs, &conf->camMain);
    {
        RenderState rsModel = rs;
        MulModelScale(&rsModel, (vec3){0.5f, 0.5f, 0.5f}, (vec3){0.5f, 0.5f, 0.5f});
        RenderModel(&rsModel, conf, frame, &MDL_SPHERES);
    }
    {
        RenderState rsModel = rs;
        MulModelPosition(&rsModel, (vec3){0.0f, -4.0f, 0.0f}, (vec3){0.0f, -4.0f, 0.0f});
        MulModelScale(&rsModel, (vec3){2.5f, 2.5f, 2.5f}, (vec3){2.5f, 2.5f, 2.5f});
        RenderModel(&rsModel, conf, frame, &MDL_SPONZA);
    }

    StartRenderPass(&rs, "GBuffer lighting");
    BindFramebuffer(FB_ONLY_COLOR_HDR);
    SetRenderProgram(&rs, &PROG_GBUF_LIGHTING);
    SetCamera(&rs, &conf->camMain);
    // Ambient lighting:
    float i = 0.05f;
    glUniform3fv(UNIF_AMBIENT_CUBE, 6, (float[]){
        3.4f * i, 3.4f * i, 3.3f * i,  // Y+ (sky)
        0.4f * i, 0.4f * i, 0.4f * i,  // Y- (ground)
        1.05f * i, 1.1f * i, 1.0f * i,  // Z+ (north)
        1.05f * i, 1.1f * i, 1.0f * i,  // Z- (south)
        1.05f * i, 1.1f * i, 1.0f * i,  // X+ (east)
        1.05f * i, 1.1f * i, 1.0f * i,  // X- (west)
    });
    // Directional lighting:
    glUniform3f(UNIF_SUN_DIRECTION, -1.0f, -1.2f, -1.0f);
    glUniform3f(UNIF_SUN_COLOR, 2.5f, 2.5f, 2.1f);
    // Point lighting:
    glUniform3fv(UNIF_POINTLIGHT_POSITIONS, 4, (float[]){
        4.9f,  2.0f,  4.2f,
        -4.8f,  2.5f,  4.4f,
        -4.2f,  2.3f, -4.6f,
        4.1f,  2.9f, -4.5f,
    });
    glUniform3fv(UNIF_POINTLIGHT_COLORS, 4, (float[]){
        20.0f, 20.0f, 20.0f,
        20.0f, 20.0f, 20.0f,
        20.0f, 20.0f, 20.0f,
        20.0f, 20.0f, 20.0f,
    });
    RenderMesh(&rs, conf, frame, &MESH_QUAD, &MAT_FULLSCREEN_QUAD);

    StartRenderPass(&rs, "Final output");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);
    SetRenderProgram(&rs, &PROG_FINAL);
    SetCamera(&rs, &conf->camMain);
    RenderMesh(&rs, conf, frame, &MESH_QUAD, &MAT_FULLSCREEN_QUAD);

    StartRenderPass(&rs, "User interface");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);
    GUI_Render();

    // *****************************************************************************************************************
    // Swap & poll:

    double tSwapStart = glfwGetTime();
    glfwSwapBuffers(window);

    double tPollStart = glfwGetTime();
    glfwPollEvents();

    frame->tMain   = tRenderStart - frame->t;
    frame->tRender = tSwapStart - tRenderStart;
    frame->tSwap   = tPollStart - tSwapStart;
    frame->tPoll   = glfwGetTime() - tPollStart;
    frame->n++;

    // PollEvents usually takes under 1ms, so if we exceed 100ms it's probably because the
    // window is moving. Ignore the frame for timing purposes if this happens.
    if (frame->tPoll > 0.1f) {
        frame->tPoll = 0.0f;
        glfwSetTime(tPollStart);
    }
}

int main() {
    vxConfig conf = {0};
    GLFWwindow* window = NULL;
    GameLoad(&conf, &window);
    GameReload(&conf, window);
    vxFrame frame = {0};
    vxFrame lastFrame = {0};
    while (!glfwWindowShouldClose(window)) {
        GameTick(&conf, window, &frame, &lastFrame);
        lastFrame = frame;
        memset(&frame, 0, sizeof(frame));
    }
}

#if 0
int main() {
    vxEnableSignalHandlers();
    vxConfigureLogging();
    glfwSetErrorCallback(sGlfwErrorCallback);
    vxCheck(glfwInit());

    static vxConfig conf;
    vxConfig_Init(&conf);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, true);
    glfwWindowHint(GLFW_SRGB_CAPABLE, true);
    GLFWwindow* window = glfwCreateWindow(conf.displayW, conf.displayH, "VX", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGL();
    #ifdef GLAD_DEBUG
    glad_set_post_callback(sGladPostCallback);
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
    GUI_Init(window);
    GUI_RenderLoadingFrame(window, "Loading...", "", 0.2f, 0.3f, 0.4f, 1.0f, 1.0f, 1.0f);
    // Shaders:
    #define X(name, path) name = LoadShaderFromDisk(#name, path);
    XM_ASSETS_SHADERS
    #undef X
    // Models:
    #define X(name, dir, file) ReadModelFromDisk(#name, &name, dir, file);
    XM_ASSETS_MODELS_GLTF
    #undef X
    // Textures:
    InitTextures();
    // Samplers:
    glGenSamplers(VXGL_SAMPLER_COUNT, VXGL_SAMPLER);

    // Window configuration:
    if (glfwExtensionSupported("WGL_EXT_swap_control_tear") ||
        glfwExtensionSupported("GLX_EXT_swap_control_tear")) {
        glfwSwapInterval(-1);
    } else {
        glfwSwapInterval(1);
    }
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    // HACK: Quick and dirty 30 FPS lock for the MacBook.
    // Locking to 60FPS or not locking at all results in HEAVY stuttering.
    #ifdef __APPLE__
    glfwSwapInterval(2);
    #endif

    MDL_DUCK.materials[0].stipple = true;
    MDL_DUCK.materials[0].stipple_hard_cutoff = 0.0f;
    MDL_DUCK.materials[0].stipple_soft_cutoff = 0.9f;

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
        conf.displayW = w;
        conf.displayH = h;
        UpdateFramebuffers(w, h, conf.shadowSize, conf.envmapSize, conf.skyboxSize);
        glViewport(0, 0, w, h);

        #if 0
        const float dist = 3.0f;
        double rt = glfwGetTime() * 1.0f;
        float angle_h = rt * 0.5f;
        float angle_v = (M_PI * 0.5f) + cosf(rt * 0.5f) * 0.35f;
        // NOTE: this is a standard spherical-to-cartesian coordinate mapping with
        //   radius/rho = dist, theta = angle_v, phi = angle_h, and the Y/Z axes swapped
        G_MainCamera.position[0] = dist * sinf(angle_v) * cosf(angle_h);
        G_MainCamera.position[1] = dist * cosf(angle_v);
        G_MainCamera.position[2] = dist * sinf(angle_v) * sinf(angle_h);
        glm_vec3_add(G_MainCamera.position, G_MainCamera.target, G_MainCamera.position);
        UpdateCameraMatrices(&G_MainCamera, w, h);
        #endif

        #if 0
        MDL_DUCK.materials[0].const_diffuse[3] = (sin(3.0 * t) + 1.0) * 0.51;
        #endif

        bool cursorLocked = (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED);
        if (!cursorLocked && (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            cursorLocked = true;
        }
        if (cursorLocked && glfwGetKey(window, GLFW_KEY_ESCAPE)) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            cursorLocked = false;
        }

        static float ry = 0;
        static float rx = 0;
        double mx, my;
        static float lmx = 0;
        static float lmy = 0;
        glfwGetCursorPos(window, &mx, &my);
        float dmx = lmx - (float) mx;
        float dmy = lmy - (float) my;
        if (cursorLocked) {
            const float sx = -0.002f;
            const float sy = -0.002f;
            ry += dmx * sx;
            rx += dmy * sy;
            rx = vxClamp(rx, vxRadians(-90.0f), vxRadians(90.0f));
            if (ry < vxRadians(-360)) ry += vxRadians(360);
            if (ry > vxRadians(+360)) ry -= vxRadians(360);
        }
        lmx = (float) mx;
        lmy = (float) my;

        vec4 q  = GLM_QUAT_IDENTITY_INIT;
        vec4 qy = GLM_QUAT_IDENTITY_INIT;
        vec4 qx = GLM_QUAT_IDENTITY_INIT;
        glm_quat(qy, ry, 0, 1, 0);
        glm_quat(qx, rx, 1, 0, 0);
        glm_quat_mul(qx, qy, q);

        static vec3 pos = {-2, -2, -2};
        vec3 spd = {-0.14f, -0.18f, -0.14f};
        vec3 dpos = GLM_VEC3_ZERO_INIT;
        if (glfwGetKey(window, GLFW_KEY_SPACE)      == GLFW_PRESS) { pos[1] += spd[1]; }
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) { pos[1] -= spd[1]; }
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { dpos[2] = -1; }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { dpos[2] = +1; }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { dpos[0] = -1; }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { dpos[0] = +1; }
        glm_vec3_mul(dpos, spd, dpos);
        glm_vec3_rotate(pos, ry, (vec3){0, 1, 0});
        glm_vec3_add(pos, dpos, pos);
        glm_vec3_rotate(pos, -ry, (vec3){0, 1, 0});

        mat4 vmat;
        glm_quat_mat4(q, vmat);
        glm_translate(vmat, pos);
        Camera_Update(&conf.camMain, w, h, vmat);

        tFrameRenderStart = glfwGetTime();

        StartRenderPass("GBuffer clear");
        BindFramebuffer(FB_GBUFFER);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearDepth(0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        StartRenderPass("GBuffer render");
        BindFramebuffer(FB_GBUFFER);
        SetCameraMatrices(&conf.camMain);
        SetRenderProgram(VSH_DEFAULT, FSH_GBUF_MAIN);

        ResetModelMatrix();
        AddModelScale((vec3){0.5f, 0.5f, 0.5f}, (vec3){0.5f, 0.5f, 0.5f});
        RenderModel(&MDL_SPHERES, w, h, t, vxFrameNumber);

        // ResetModelMatrix();
        // AddModelPosition((vec3){-0.1f, -0.5f, 0.0f}, (vec3){-0.1f, -0.5f, 0.0f});
        // RenderModel(&MDL_DUCK, w, h, t, vxFrameNumber);

        // ResetModelMatrix();
        // AddModelPosition((vec3){0.0f, -1.0f, 0.0f}, (vec3){0.0f, -1.0f, 0.0f});
        // AddModelScale((vec3){1.2f, 1.2f, 1.2f}, (vec3){1.2f, 1.2f, 1.2f});
        // RenderModel(&MDL_BOX_MR, w, h, t, vxFrameNumber);

        ResetModelMatrix();
        AddModelPosition((vec3){0.0f, -4.0f, 0.0f}, (vec3){0.0f, -4.0f, 0.0f});
        AddModelScale((vec3){2.5f, 2.5f, 2.5f}, (vec3){2.5f, 2.5f, 2.5f});
        RenderModel(&MDL_SPONZA, w, h, t, vxFrameNumber);

        StartRenderPass("GBuffer lighting");
        BindFramebuffer(FB_ONLY_COLOR_HDR);
        SetRenderProgram(VSH_FULLSCREEN_PASS, FSH_GBUF_LIGHTING);
        SetCameraMatrices(&conf.camMain);
        // Ambient lighting:
        float i = 0.05f;
        glUniform3fv(UNIF_AMBIENT_CUBE, 6, (float[]){
            3.4f * i, 3.4f * i, 3.3f * i,  // Y+ (sky)
            0.4f * i, 0.4f * i, 0.4f * i,  // Y- (ground)
            1.05f * i, 1.1f * i, 1.0f * i,  // Z+ (north)
            1.05f * i, 1.1f * i, 1.0f * i,  // Z- (south)
            1.05f * i, 1.1f * i, 1.0f * i,  // X+ (east)
            1.05f * i, 1.1f * i, 1.0f * i,  // X- (west)
        });
        // Directional lighting:
        glUniform3f(UNIF_SUN_DIRECTION, -1.0f, -1.2f, -1.0f);
        glUniform3f(UNIF_SUN_COLOR, 2.5f, 2.5f, 2.1f);
        // Point lighting:
        glUniform3fv(UNIF_POINTLIGHT_POSITIONS, 4, (float[]){
             4.9f,  2.0f,  4.2f,
            -4.8f,  2.5f,  4.4f,
            -4.2f,  2.3f, -4.6f,
             4.1f,  2.9f, -4.5f,
        });
        glUniform3fv(UNIF_POINTLIGHT_COLORS, 4, (float[]){
            20.0f, 20.0f, 20.0f,
            20.0f, 20.0f, 20.0f,
            20.0f, 20.0f, 20.0f,
            20.0f, 20.0f, 20.0f,
        });
        // Run pass:
        RunFullscreenPass(w, h, t, vxFrameNumber);

        StartRenderPass("Skybox");
        BindFramebuffer(FB_ONLY_COLOR_HDR);
        SetRenderProgram(VSH_SKYBOX, FSH_SKYBOX);
        SetUniformCubemap(UNIF_TEX_ENVMAP, ENVMAP_CUBE, GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE);
        SetCameraMatrices(&conf.camMain);
        vec3 viewPos;
        glm_vec3_copy(conf.camMain.view_matrix[3], viewPos); // extract translation
        ResetModelMatrix();
        AddModelPosition(viewPos, viewPos);
        AddModelScale((vec3){1000,1000,1000}, (vec3){1000,1000,1000});
        RenderModel(&MDL_BOX_MR, w, h,t, vxFrameNumber);

        // StartRenderPass("Dither Effect");
        // BindFramebuffer(FB_ONLY_COLOR_LDR);
        // SetRenderProgram(VSH_FULLSCREEN_PASS, FSH_FX_DITHER);
        // RunFullscreenPass(w, h, t, vxFrameNumber);

        StartRenderPass("Final Pass");
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        SetRenderProgram(VSH_FULLSCREEN_PASS, FSH_FINAL);
        SetCameraMatrices(&conf.camMain);
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
#endif