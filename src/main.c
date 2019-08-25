#include "main.h"
#include "gui/gui.h"
#include "flib/accessor.h"
#include "data/camera.h"
#include "data/texture.h"
#include "render/render.h"
#include "render/program.h"
#include "scene/core.h"
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
    c->clearColorBuffers = false;
    c->gpuSupportsClipControl = false;

    c->tonemapMode = TONEMAP_HABLE;
    c->tonemapExposure = 16.0f;
    c->tonemapACESParamA = 2.51f;
    c->tonemapACESParamB = 0.03f;
    c->tonemapACESParamC = 2.43f;
    c->tonemapACESParamD = 0.59f;
    c->tonemapACESParamE = 0.14f;

    c->debugVisMode = DEBUG_VIS_NONE;
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

// Loads the default scene. Should only be run once.
void GameLoadScene (Scene* scene) {
    GameObject* sponza = AddObject(scene, NULL, GAMEOBJECT_MODEL);
    sponza->model.model = &MDL_SPONZA;
    sponza->localScale[0] = 2.5f;
    sponza->localScale[1] = 2.5f;
    sponza->localScale[2] = 2.5f;
    sponza->localPosition[0] = +2.5f;
    sponza->localPosition[1] = -4.0f;
    sponza->localPosition[2] = +2.5f;
    GameObject* duck = AddObject(scene, sponza, GAMEOBJECT_MODEL);
    duck->model.model = &MDL_DUCK;
    duck->localPosition[1] = 2.0f;
    duck->localScale[0] = 1/sponza->localScale[0];
    duck->localScale[1] = 1/sponza->localScale[1];
    duck->localScale[2] = 1/sponza->localScale[2];
}

// Tick function for the game. Generates a single frame.
void GameTick (vxConfig* conf, GLFWwindow* window, vxFrame* frame, vxFrame* lastFrame, Scene* scene) {
    frame->t = (float) glfwGetTime();
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
    UpdateScene(scene);
    GUI_StartFrame();

    // Debug user interface:
    GUI_DrawStatistics(lastFrame);
    GUI_DrawDebugUI(conf, window, scene);
    bool uiWantsInput = GUI_InterfaceWantsInput();

    // Manage cursor lock status:
    // TODO: don't affect ImGui elements when locked
    bool cursorLocked = (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED);
    if (!cursorLocked && !uiWantsInput && (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)) {
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
        static float ry = vxRadians(-45);
        static float rx = vxRadians(20);
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

        static vec3 pos = {-3, -4, -3};
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
    
    // *****************************************************************************************************************
    // Render:

    double tRenderStart = glfwGetTime();
    static RenderState rs;
    glViewport(0, 0, conf->displayW, conf->displayH);

    static RenderList rl = {0};
    UpdateRenderList(&rl, scene);

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
    #if 0
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
    #else
    RenderState rsMesh = rs;
    for (size_t i = 0; i < rl.meshCount; i++) {
        glm_mat4_copy(rl.meshes[i].worldMatrix, rsMesh.matModel);
        glm_mat4_copy(rl.meshes[i].lastWorldMatrix, rsMesh.matModelLast);
        RenderMesh(&rsMesh, conf, frame, &rl.meshes[i].mesh, rl.meshes[i].material);
    }
    #endif

    StartRenderPass(&rs, "GBuffer lighting");
    BindFramebuffer(FB_ONLY_COLOR_HDR);
    SetRenderProgram(&rs, &PROG_GBUF_LIGHTING);
    SetCamera(&rs, &conf->camMain);
    // Ambient lighting:
    float i = 0.01f;
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
    glUniform3f(UNIF_SUN_COLOR, 1.0f, 1.0f, 0.9f);
    // Point lighting:
    glUniform3fv(UNIF_POINTLIGHT_POSITIONS, 4, (float[]){
        4.9f,  2.0f,  4.2f,
        -4.8f,  2.5f,  4.4f,
        -4.2f,  2.3f, -4.6f,
        4.1f,  2.9f, -4.5f,
    });
    glUniform3fv(UNIF_POINTLIGHT_COLORS, 4, (float[]){
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
    });
    RenderMesh(&rs, conf, frame, &MESH_QUAD, &MAT_FULLSCREEN_QUAD);

    StartRenderPass(&rs, "Final output");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);
    glEnable(GL_FRAMEBUFFER_SRGB);
    SetRenderProgram(&rs, &PROG_FINAL);
    SetCamera(&rs, &conf->camMain);
    glUniform1f(UNIF_TONEMAP_EXPOSURE, conf->tonemapExposure);
    if (conf->tonemapMode == TONEMAP_ACES) {
        glUniform1f(UNIF_TONEMAP_ACES_PARAM_A, conf->tonemapACESParamA);
        glUniform1f(UNIF_TONEMAP_ACES_PARAM_B, conf->tonemapACESParamB);
        glUniform1f(UNIF_TONEMAP_ACES_PARAM_C, conf->tonemapACESParamC);
        glUniform1f(UNIF_TONEMAP_ACES_PARAM_D, conf->tonemapACESParamD);
        glUniform1f(UNIF_TONEMAP_ACES_PARAM_E, conf->tonemapACESParamE);
    }
    RenderMesh(&rs, conf, frame, &MESH_QUAD, &MAT_FULLSCREEN_QUAD);
    glDisable(GL_FRAMEBUFFER_SRGB);

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

    frame->tMain   = (float)(tRenderStart - frame->t);
    frame->tRender = (float)(tSwapStart - tRenderStart);
    frame->tSwap   = (float)(tPollStart - tSwapStart);
    frame->tPoll   = (float)(glfwGetTime() - tPollStart);
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

    Scene scene = {0};
    GameLoadScene(&scene);

    vxFrame frame = {0};
    vxFrame lastFrame = {0};
    while (!glfwWindowShouldClose(window)) {
        GameTick(&conf, window, &frame, &lastFrame, &scene);
        lastFrame = frame;
        memset(&frame, 0, sizeof(frame));
    }
}