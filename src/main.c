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
    InitScene(scene);
    
    GameObject* sponza = AddObject(scene, NULL, GAMEOBJECT_MODEL);
    sponza->model.model = &MDL_SPONZA;
    sponza->localScale[0] = 2.5f;
    sponza->localScale[1] = 2.5f;
    sponza->localScale[2] = 2.5f;
    sponza->localPosition[0] = +1.5f;
    sponza->localPosition[1] = -4.0f;
    sponza->localPosition[2] = +1.5f;

    GameObject* duck = AddObject(scene, sponza, GAMEOBJECT_MODEL);
    duck->model.model = &MDL_DUCK;
    duck->localPosition[1] = 2.0f;
    duck->localScale[0] = 1/sponza->localScale[0];
    duck->localScale[1] = 1/sponza->localScale[1];
    duck->localScale[2] = 1/sponza->localScale[2];

    GameObject* sunlight = AddObject(scene, NULL, GAMEOBJECT_DIRECTIONAL_LIGHT);
    sunlight->localPosition[0] = +1.0f;
    sunlight->localPosition[1] = +1.0f;
    sunlight->localPosition[2] = +1.0f;
    sunlight->directionalLight.color[0] = 2.0f;
    sunlight->directionalLight.color[1] = 2.0f;
    sunlight->directionalLight.color[2] = 1.8f;

    GameObject* p1 = AddObject(scene, NULL, GAMEOBJECT_POINT_LIGHT);
    p1->localPosition[0] = 4.0f;
    p1->localPosition[1] = 0.0f;
    p1->localPosition[2] = 0.0f;
    p1->pointLight.color[0] = 10.0f;
    p1->pointLight.color[1] = 10.0f;
    p1->pointLight.color[2] = 10.0f;

    GameObject* p2 = AddObject(scene, NULL, GAMEOBJECT_POINT_LIGHT);
    p2->localPosition[0] = -4.0f;
    p2->localPosition[1] = 0.0f;
    p2->localPosition[2] = 0.0f;
    p2->pointLight.color[0] = 10.0f;
    p2->pointLight.color[1] = 10.0f;
    p2->pointLight.color[2] = 10.0f;

    GameObject* lp = AddObject(scene, NULL, GAMEOBJECT_LIGHT_PROBE);
    lp->localPosition[0] = 0.0f;
    lp->localPosition[1] = 0.0f;
    lp->localPosition[2] = 0.0f;
    const float mul = 0.05f;
    glm_vec3_copy((vec3){mul*1.0f, mul*1.0f, mul*1.0f}, lp->lightProbe.colorXp);
    glm_vec3_copy((vec3){mul*1.0f, mul*1.0f, mul*1.0f}, lp->lightProbe.colorXn);
    glm_vec3_copy((vec3){mul*1.5f, mul*1.5f, mul*1.5f}, lp->lightProbe.colorYp);
    glm_vec3_copy((vec3){mul*0.5f, mul*0.5f, mul*0.5f}, lp->lightProbe.colorYn);
    glm_vec3_copy((vec3){mul*1.0f, mul*1.0f, mul*1.0f}, lp->lightProbe.colorZp);
    glm_vec3_copy((vec3){mul*1.0f, mul*1.0f, mul*1.0f}, lp->lightProbe.colorZn);
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

    StartBlock("Frame Update");

    // Retrieve new framebuffer size and update framebuffers if required:
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    conf->displayW = w;
    conf->displayH = h;
    StartGPUBlock("Update Render Targets");
    uint8_t updatedTargets = UpdateRenderTargets(conf);
    EndGPUBlock();
    glViewport(0, 0, w, h);
    
    // Run subsystem tick functions:
    TimedBlock("Update Programs",  UpdatePrograms(conf));
    TimedBlock("Update Scene",     UpdateScene(scene));
    TimedBlock("ImGui StartFrame", GUI_StartFrame());

    // Debug user interface:
    StartBlock("GUI Update");
    GUI_DrawStatistics(lastFrame);
    GUI_DrawDebugUI(conf, window, scene);
    bool uiWantsInput = GUI_InterfaceWantsInput();
    EndBlock();

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
    StartBlock("FPS Camera Control");
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

        static vec3 pos = {3, 4, 3};
        vec3 spd = {8.0f * frame->dt, 8.0f * frame->dt, 8.0f * frame->dt};
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
        glm_vec3_negate(pos);
        glm_translate(vmat, pos);
        glm_vec3_negate(pos);
        Camera_Update(&conf->camMain, w, h, vmat);
    }
    EndBlock();
    frame->mouseX = (float) mx;
    frame->mouseY = (float) my;
    frame->mouseDx = (float) dmx;
    frame->mouseDy = (float) dmy;
    
    EndBlock();
    
    // *****************************************************************************************************************
    // Render:

    StartBlock("Frame Render");

    double tRenderStart = glfwGetTime();
    static RenderState rs;
    glViewport(0, 0, conf->displayW, conf->displayH);

    // tRenderStart to tSwapStart will just be the CPU time taken up by drawcall submission. To monitor GPU time we need
    // to use an OpenGL query object: https://www.khronos.org/opengl/wiki/Query_Object
    // We use multiple query objects to try to avoid stalls due to results being delayed one or more frames.
    static int rtqIndex = 0;
    static GLuint rtq [4] = {0};
    if (rtq[0] == 0) {
        glGenQueries(4, &rtq);
    }
    glBeginQuery(GL_TIME_ELAPSED, rtq[rtqIndex]);

    static RenderList rl = {0};
    TimedBlock("UpdateRenderList", {
        UpdateRenderList(&rl, scene);
    });

    if (updatedTargets & UPDATED_ENVMAP_TARGETS) {
        // TODO: generate environment maps
    }
    
    if (conf->clearColorBuffers) {
        RenderPass(&rs, "GBuffer clear (color+depth)", {
            BindFramebuffer(FB_GBUFFER);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClearDepth(0.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        });
    } else {
        RenderPass(&rs, "GBuffer clear (depth-only)", {
            BindFramebuffer(FB_GBUFFER);
            glClearDepth(0.0f);
            glClear(GL_DEPTH_BUFFER_BIT);
        });
    }

    StartRenderPass(&rs, "GBuffer main (opaque objects)");
    BindFramebuffer(FB_GBUFFER);
    SetRenderProgram(&rs, &PROG_GBUF_MAIN);
    SetCamera(&rs, &conf->camMain);
    RenderState rsMesh = rs;
    for (size_t i = 0; i < rl.meshCount; i++) {
        glm_mat4_copy(rl.meshes[i].worldMatrix, rsMesh.matModel);
        glm_mat4_copy(rl.meshes[i].lastWorldMatrix, rsMesh.matModelLast);
        #if 0
        static char blockName [128];
        int written = stbsp_snprintf(blockName, 128, "RenderMesh %u (%ju tris)",
            rl.meshes[i].mesh.gl_vertex_array,
            rl.meshes[i].mesh.gl_element_count / 3);
        blockName[written] = '\0';
        StartGPUBlock(blockName);
        #endif
        RenderMesh(&rsMesh, conf, frame, &rl.meshes[i].mesh, rl.meshes[i].material);
        #if 0
        EndGPUBlock();
        #endif
    }
    EndRenderPass();

    StartRenderPass(&rs, "GBuffer lighting");
    BindFramebuffer(FB_ONLY_COLOR_HDR);
    SetRenderProgram(&rs, &PROG_GBUF_LIGHTING);
    SetCamera(&rs, &conf->camMain);
    // Extract light info from scene:
    RenderableLightProbe* ambient = NULL;
    RenderableDirectionalLight* directional = NULL;
    vec3 pointPositions[4] = {0};
    vec3 pointColors[4]    = {0};
    if (rl.lightProbeCount > 0) {
        ambient = &rl.lightProbes[0];
    }
    if (rl.directionalLightCount > 0) {
        directional = &rl.directionalLights[0];
    }
    for (int i = 0; i < vxMin(4, rl.pointLightCount); i++) {
        glm_vec3_copy(rl.pointLights[i].position, pointPositions[i]);
        glm_vec3_copy(rl.pointLights[i].color, pointColors[i]);
    }
    // Send light uniforms:
    if (ambient) {
        glUniform3fv(UNIF_AMBIENT_CUBE, 6, (float*) ambient->colors);
    }
    if (directional) {
        glUniform3fv(UNIF_SUN_POSITION, 1, directional->position);
        glUniform3fv(UNIF_SUN_COLOR, 1, directional->color);
    }
    glUniform3fv(UNIF_POINTLIGHT_POSITIONS, 4, (float*) pointPositions);
    glUniform3fv(UNIF_POINTLIGHT_COLORS, 4, (float*) pointColors);
    RenderMesh(&rs, conf, frame, &MESH_QUAD, &MAT_FULLSCREEN_QUAD);
    EndRenderPass();

    StartRenderPass(&rs, "Debug: Point Light Positions");
    // We do the binding manually to avoid PROG_GBUF_MAIN wiping out any data in gAux2.
    glBindFramebuffer(GL_FRAMEBUFFER, FB_ONLY_COLOR_HDR);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    SetRenderProgram(&rs, &PROG_GBUF_MAIN);
    SetCamera(&rs, &conf->camMain);
    for (int i = 0; i < rl.pointLightCount; i++) {
        const float scale = 0.2f;
        RenderState cubeRs = rs;
        MulModelPosition(&cubeRs, rl.pointLights[i].position, rl.pointLights[i].position);
        MulModelScale(&cubeRs, (vec3){scale, scale, scale}, (vec3){scale, scale, scale});
        RenderMesh(&cubeRs, conf, frame, &MESH_CUBE, &MAT_DIFFUSE_WHITE);
    }
    EndRenderPass();

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
    EndRenderPass();

    RenderPass(&rs, "User interface", {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);
        GUI_Render();
    });

    EndBlock();

    StartBlock("OpenGL render time query");
    glEndQuery(GL_TIME_ELAPSED);
    int64_t dtOpenGLRender = 0;
    // glGetQueryObject will produce INVALID_OPERATION errors if called for queries that have never been started.
    // During the first frames, we'll just query the current frame's render time (synchronously). After all query
    // objects become valid, we can switch to asynchronous queries (N frames behind).
    if (frame->n >= 4) {
        rtqIndex = (rtqIndex + 1) % 4;
        glGetQueryObjecti64v(rtq[rtqIndex], GL_QUERY_RESULT, &dtOpenGLRender);
    } else {
        glGetQueryObjecti64v(rtq[rtqIndex], GL_QUERY_RESULT, &dtOpenGLRender);
        rtqIndex = (rtqIndex + 1) % 4;
    }
    EndBlock();

    // *****************************************************************************************************************
    // Swap & poll:

    double tSwapStart = glfwGetTime();
    TimedGPUBlock("Swap buffers", {
        glfwSwapBuffers(window);
    });

    double tPollStart = glfwGetTime();
    TimedBlock("Poll events", {
        glfwPollEvents();
    });

    TimedBlock("FrameTiming", {
        // All of these values are supposed to be in seconds.
        frame->tMain   = (float)(tRenderStart - frame->t);
        frame->tSubmit = (float)(tSwapStart - tRenderStart);
        frame->tRender = (float)(dtOpenGLRender) / 1000000000.0f; // nanoseconds
        frame->tSwap   = (float)(tPollStart - tSwapStart);
        frame->tPoll   = (float)(glfwGetTime() - tPollStart);
        frame->n++;

        // PollEvents usually takes under 1ms, so if we exceed 100ms it's probably because the
        // window is moving. Ignore the frame for timing purposes if this happens.
        if (frame->tPoll > 0.1f) {
            frame->tPoll = 0.0f;
            glfwSetTime(tPollStart);
        }
    });
}

int main() {
    vxConfig conf = {0};
    GLFWwindow* window = NULL;

    Remotery* rmt;
    rmt_CreateGlobalInstance(&rmt);

    TimedBlock("GameLoad", GameLoad(&conf, &window));
    rmt_BindOpenGL();
    
    TimedBlock("GameReload", GameReload(&conf, window));

    Scene scene = {0};
    TimedBlock("GameLoadScene", GameLoadScene(&scene));

    vxFrame frame = {0};
    vxFrame lastFrame = {0};
    while (!glfwWindowShouldClose(window)) {
        TimedBlock("Frame", {
            TimedBlock("GameTick", GameTick(&conf, window, &frame, &lastFrame, &scene));
            lastFrame = frame;
            memset(&frame, 0, sizeof(frame));
        });
    }

    rmt_UnbindOpenGL();
    rmt_DestroyGlobalInstance(rmt);
}