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

static void sGlfwErrorCallback (int code, const char* error) {
    vxLog("GLFW error %d: %s", code, error);
}

#ifdef GLAD_DEBUG
static void sGladPostCallback (const char *name, void *funcptr, int len_args, ...) {
    GLenum error;
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

// Initializes the engine's configuration struct.
// This function is where default configuration values are defined. To add a new option, you should:
// * Update the vxConfig struct (main.h)
// * Add a default value for the new option (here), or just let this function initialize it to 0
// * Add a UI field for it in sDrawConfigurator (gui.cc)
// * If shaders need to be aware of it, update sGenerateDefineBlock (program.c) to add a define for it
void vxConfig_Init (vxConfig* c) {
    memset(c, 0, sizeof(vxConfig));

    #ifdef __APPLE__
    // HACK: 30 FPS lock for the MacBook
    // Locking to 60FPS and missing frames, or not locking at all, results in heavy stuttering. It seems like the macOS
    // compositor expects one frame per VSync interval and just drops anything else.
    c->swapInterval = 2;
    #else
    c->swapInterval = -1;
    #endif

    c->displayW = 1280;
    c->displayH = 1024;
    c->envmapSize = 512;
    c->skyboxSize = 2048;

    // zn=0.5f results in reasonably high depth precision even without clip-control support.
    Camera_InitPerspective(&c->camMain, 0.5f, 0.0f, 80.0f);

    // These are meant to be used for dynamic cubemap generation, but I probably won't get to that in time.
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
    c->clearColorBuffers = true;

    // We use glClipControl/glDepthRangedNV when possible to tell OpenGL that the clip-space depth values we generate
    // are in the [0,1] range rather than the default [-1,1] range.
    // If glClipControl is not available, OpenGL will squish our [0,1] range into [0.5,1] when writing to the depth
    // buffer, destroying most of its precision (which is close to 0). We can still work with that, though.
    // This flag is set in GameInit, since we need an OpenGL context to list extensions.
    c->gpuSupportsClipControl = false;    

    c->tonemapMode = TONEMAP_HABLE;
    c->tonemapExposure = 16.0f;
    c->tonemapACESParamA = 2.51f;
    c->tonemapACESParamB = 0.03f;
    c->tonemapACESParamC = 2.43f;
    c->tonemapACESParamD = 0.59f;
    c->tonemapACESParamE = 0.14f;

    c->debugVisMode = DEBUG_VIS_NONE;

    c->shadowSize = 4096;
    c->shadowBiasMin = 0.0002f;
    c->shadowBiasMax = 0.01f;
    c->shadowHoverFix = false;
    c->shadowPcfTapsX = 2;
    c->shadowPcfTapsY = 2;
    Camera_InitOrtho(&c->camShadow, 100.0f, -1000.0f, 500.0f); // no idea why these values work
}

// Halton(2,3) 8-sample offset sequence used for TAA. Initialized by GameLoad.
typedef float Halton2x3x8T [2*8];
Halton2x3x8T Halton2x3x8;

// See https://github.com/playdeadgames/temporal/blob/master/Assets/Scripts/FrustumJitter.cs (HaltonSeq)
static float HaltonSeq (int prime, int index) {
    float r = 0.0f;
    float f = 1.0f;
    int i = index;
    while (i > 0) {
        f /= prime;
        r += f * (i % prime);
        i = (int) floorf((float)i / (float)prime);
    }
    return r;
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

    // Figure out if this machine supports glClipControl:
    if (glfwExtensionSupported("GL_ARB_clip_control") || glfwExtensionSupported("NV_depth_buffer_float")) {
        conf->gpuSupportsClipControl = true;
    } else {
        conf->gpuSupportsClipControl = false;
        vxLog("Warning: This machine's graphics driver doesn't support glClipControl.");
    }

    // Window configuration:
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    // Generate Halton(2,3) sequence:
    for (int i = 0; i < vxSize(Halton2x3x8) / 2; i++) {
        Halton2x3x8[2*i+0] = HaltonSeq(2, i+1) - 0.5f;
        Halton2x3x8[2*i+1] = HaltonSeq(3, i+1) - 0.5f;
    }

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

// Loads the default scene.
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
    MDL_DUCK.materials[0].stipple = true;
    MDL_DUCK.materials[0].stipple_hard_cutoff = 0.0f;
    MDL_DUCK.materials[0].stipple_soft_cutoff = 1.0f;
    MDL_DUCK.materials[0].const_diffuse[3] = 0.6f;

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

// Tick function for the game. Renders a single frame.
void GameTick (vxConfig* conf, GLFWwindow* window, vxFrame* frame, vxFrame* lastFrame, Scene* scene) {
    frame->t = (float) glfwGetTime();
    frame->dt = frame->t - lastFrame->t;

    // Pause the game if it loses focus:
    if (conf->pauseOnFocusLoss && !glfwGetWindowAttrib(window, GLFW_FOCUSED)) {
        glfwWaitEvents();
        glfwSetTime(frame->t);
        return;
    }

    // Print the last frame's log:
    vxAdvanceFrame();
    frame->n = vxFrameNumber;

    // *****************************************************************************************************************
    // Update:

    StartBlock("Frame Update");
    StartFrame(conf, window); // applies swap interval and clip-control settings

    // Retrieve new framebuffer size and update framebuffers if required:
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    conf->displayW = w;
    conf->displayH = h;
    StartGPUBlock("Update Render Targets");
    uint8_t updatedTargets = UpdateRenderTargets(conf); // resizes framebuffer render targets
    EndGPUBlock();
    
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
    static vec3 pos = {3, 4, 3}; // player (camera) position
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
            const float sx = -0.002f; // player look speed (horizontal)
            const float sy = -0.002f; // player look speed (vertical)
            ry += dmx * sx;
            rx += dmy * sy;
            rx = vxClamp(rx, vxRadians(-90.0f), vxRadians(90.0f));
            if (ry < vxRadians(-360)) ry += vxRadians(360);
            if (ry > vxRadians(+360)) ry -= vxRadians(360);
        }
        lmx = (float) mx;
        lmy = (float) my;

        vec3 spd = {8.0f * frame->dt, 8.0f * frame->dt, 8.0f * frame->dt}; // player movement speed
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

        vec4 q  = GLM_QUAT_IDENTITY_INIT;
        vec4 qy = GLM_QUAT_IDENTITY_INIT;
        vec4 qx = GLM_QUAT_IDENTITY_INIT;
        glm_quat(qy, ry, 0, 1, 0);
        glm_quat(qx, rx, 1, 0, 0);
        glm_quat_mul(qx, qy, q);

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

    // tRenderStart to tRenderEnd will just be the CPU time taken up by drawcall submission. To monitor GPU time we need
    // to use an OpenGL query object: https://www.khronos.org/opengl/wiki/Query_Object
    // We use multiple query objects in order to avoid stalling the renderer just to wait for a reply from the GPU.
    static int rtqIndex = 0;
    static GLuint rtq [4] = {0};
    if (rtq[0] == 0) {
        glGenQueries(4, &rtq);
    }
    glBeginQuery(GL_TIME_ELAPSED, rtq[rtqIndex]);

    // Render lists contain abbreviated entries for game objects that affect the rendered image.
    static RenderList rl = {0};
    TimedBlock("UpdateRenderList", {
        UpdateRenderList(&rl, scene);
    });

    if (updatedTargets & UPDATED_ENVMAP_TARGETS) {
        // TODO: generate environment maps
    }

    // Don't forget to set the correct viewports!
    glViewport(0, 0, conf->shadowSize, conf->shadowSize);

    RenderPass(&rs, "Shadow Map Clear", {
        BindFramebuffer(FB_SHADOW);
        glClearDepth(0.0f);
        glClear(GL_DEPTH_BUFFER_BIT);
    })

    // Right now we only support one directional light.
    RenderableDirectionalLight* directional = NULL;
    if (rl.directionalLightCount > 0) {
        directional = &rl.directionalLights[0];
        // Update shadow camera:
        vec3 camPos;
        // We discretize the player's position as seen by the camera in order to minimize shadow crawling.
        vec3 playerPos;
        static const int factor = 5;
        playerPos[0] = (float)((int)pos[0] / factor);
        playerPos[1] = (float)((int)pos[1] / factor);
        playerPos[2] = (float)((int)pos[2] / factor);
        glm_vec3_sub(playerPos, directional->position, camPos);
        mat4 vmat;
        glm_lookat(camPos, playerPos, VX_UP, vmat);
        Camera_Update(&conf->camShadow, conf->shadowSize, conf->shadowSize, vmat);
        // Render shadowmap:
        StartRenderPass(&rs, "Shadow Map");
        BindFramebuffer(FB_SHADOW);
        SetRenderProgram(&rs, &PROG_SHADOW);
        SetCamera(&rs, &conf->camShadow);
        RenderState rsMesh = rs;
        if (conf->shadowHoverFix) {
            // This is supposed to mitigate the shadow "Peter Panning" effect, but I can't tell the difference.
            rsMesh.forceCullFace = GL_FRONT;
        }
        for (size_t i = 0; i < rl.meshCount; i++) {
            glm_mat4_copy(rl.meshes[i].worldMatrix, rsMesh.matModel);
            RenderMesh(&rsMesh, conf, frame, &rl.meshes[i].mesh, rl.meshes[i].material);
        }
        EndRenderPass();
    }

    glViewport(0, 0, conf->displayW, conf->displayH);
    
    // The depth buffer needs to be cleared to 0 (since we use reverse depth). Other buffers can be ignored.
    // We do have an option to clear them, but that's only really useful when debugging (e.g. with RenderDoc).
    if (conf->clearColorBuffers) {
        RenderPass(&rs, "GBuffer clear (color+depth)", {
            BindFramebuffer(FB_GBUFFER);
            glClearColor(0.1f, 0.2f, 0.5f, 1.0f); // HDR
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

    // Compute main camera jitter, for TAA:
    static Camera camMainJittered;
    memcpy(&camMainJittered, &conf->camMain, sizeof(Camera));
    // float jitterScale = 0.5;
    // float jitterX, jitterY;
    // switch ((frame->n + 1) % 8) {
    //     case 0: { jitterX = +jitterScale / (float) w; jitterY = +jitterScale / (float) h; } break;
    //     case 1: { jitterX = +jitterScale / (float) w; jitterY = -jitterScale / (float) h; } break;
    //     case 2: { jitterX = -jitterScale / (float) w; jitterY = +jitterScale / (float) h; } break;
    //     case 3: { jitterX = -jitterScale / (float) w; jitterY = -jitterScale / (float) h; } break;
    //     case 4: { jitterX = 0;                        jitterY = +jitterScale / (float) h; } break;
    //     case 5: { jitterX = 0;                        jitterY = 0;                        } break;
    //     case 6: { jitterX = -jitterScale / (float) w; jitterY = +jitterScale / (float) h; } break;
    //     case 7: { jitterX = -jitterScale / (float) w; jitterY = 0;                        } break;
    // }
    // float jitterLastX, jitterLastY;
    // switch (frame->n % 8) {
    //     case 0: { jitterLastX = +jitterScale / (float) w; jitterLastY = +jitterScale / (float) h; } break;
    //     case 1: { jitterLastX = +jitterScale / (float) w; jitterLastY = -jitterScale / (float) h; } break;
    //     case 2: { jitterLastX = -jitterScale / (float) w; jitterLastY = +jitterScale / (float) h; } break;
    //     case 3: { jitterLastX = -jitterScale / (float) w; jitterLastY = -jitterScale / (float) h; } break;
    //     case 4: { jitterLastX = 0;                        jitterLastY = +jitterScale / (float) h; } break;
    //     case 5: { jitterLastX = 0;                        jitterLastY = 0;                        } break;
    //     case 6: { jitterLastX = -jitterScale / (float) w; jitterLastY = +jitterScale / (float) h; } break;
    //     case 7: { jitterLastX = -jitterScale / (float) w; jitterLastY = 0;                        } break;
    // }
    float jitterX     = Halton2x3x8[2*((frame->n+1)%8)+0] / (float)w;
    float jitterY     = Halton2x3x8[2*((frame->n+1)%8)+1] / (float)h;
    float jitterLastX = Halton2x3x8[2*((frame->n+0)%8)+0] / (float)w;
    float jitterLastY = Halton2x3x8[2*((frame->n+0)%8)+1] / (float)h;
    static mat4 jitter, jitterLast;
    glm_translate_make(jitter,     (vec3){jitterX,     jitterY,     0.0});
    glm_translate_make(jitterLast, (vec3){jitterLastX, jitterLastY, 0.0});
    glm_mat4_mul(jitter,     camMainJittered.proj_matrix,      camMainJittered.proj_matrix);
    glm_mat4_mul(jitterLast, camMainJittered.last_proj_matrix, camMainJittered.last_proj_matrix);
    glm_mat4_inv(camMainJittered.proj_matrix, camMainJittered.inv_proj_matrix);

    StartRenderPass(&rs, "GBuffer main (opaque objects)");
    BindFramebuffer(FB_GBUFFER);
    SetRenderProgram(&rs, &PROG_GBUF_MAIN);
    RenderState rsMesh = rs;
    SetCamera(&rsMesh, &camMainJittered);
    for (size_t i = 0; i < rl.meshCount; i++) {
        glm_mat4_copy(rl.meshes[i].worldMatrix,     rsMesh.matModel);
        glm_mat4_copy(rl.meshes[i].lastWorldMatrix, rsMesh.matModelLast);
        // Timing every single mesh draw is probably a waste of time, despite being cool to look at in the profiler.
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
    // Send shadow uniforms:
    mat4 shadowSpaceMatrix;
    glm_mat4_mul(conf->camShadow.proj_matrix, conf->camShadow.view_matrix, shadowSpaceMatrix);
    glUniformMatrix4fv(UNIF_SHADOW_VP_MATRIX, 1, false, (float*) shadowSpaceMatrix);
    glUniform1f(UNIF_SHADOW_BIAS_MIN, conf->shadowBiasMin);
    glUniform1f(UNIF_SHADOW_BIAS_MIN, conf->shadowBiasMin);
    // Extract light info from scene:
    RenderableLightProbe* ambient = NULL;
    vec3 pointPositions[4] = {0};
    vec3 pointColors[4]    = {0};
    if (rl.lightProbeCount > 0) {
        ambient = &rl.lightProbes[0];
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

    StartRenderPass(&rs, "Temporal AA");
    BindFramebuffer(FB_TAA);
    SetRenderProgram(&rs, &PROG_TAA);
    SetCamera(&rs, &camMainJittered);
    glUniform2f(UNIF_JITTER, jitterX, jitterY);
    glUniform2f(UNIF_JITTER_LAST, jitterLastX, jitterLastY);
    RenderMesh(&rs, conf, frame, &MESH_QUAD, &MAT_FULLSCREEN_QUAD);
    EndRenderPass();

    // I stopped writing to RT_AUX_HDR11 in the TAA shader due to some tearing artifacts, but the TAA shader has been
    // completely overhauled since, so having this as a separate step might not be needed anymore.
    StartRenderPass(&rs, "Temporal AA Accumulation Buffer Copy");
    glBindFramebuffer(GL_READ_FRAMEBUFFER, FB_TAA);
    glReadBuffer(GL_COLOR_ATTACHMENT0); // copying from RT_COLOR_HDR
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FB_TAA_COPY);
    glDrawBuffer(GL_COLOR_ATTACHMENT0); // to RT_AUX_HDR11
    glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    StartRenderPass(&rs, "Debug: Point Light Positions");
    // We do the binding manually to avoid PROG_GBUF_MAIN wiping out any data in gAux2.
    glBindFramebuffer(GL_FRAMEBUFFER, FB_ONLY_COLOR_HDR);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    // There should be a dedicated program for this kind of thing, but whatever, this works for now.
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

    double tRenderEnd = glfwGetTime();
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
        frame->tSubmit = (float)(tRenderEnd - tRenderStart);
        frame->tRender = (float)(dtOpenGLRender) / 1000000000.0f; // nanoseconds
        frame->tSwap   = (float)(tPollStart - tSwapStart);
        frame->tPoll   = (float)(glfwGetTime() - tPollStart);

        // PollEvents usually takes under 1ms, so if we exceed 100ms it's probably because the
        // window is moving. Ignore the frame for timing purposes if this happens.
        if (frame->tPoll > 0.1f) {
            frame->tPoll = 0.0f;
            glfwSetTime(tPollStart);
        }
    });
}

// Actual entry point for the game. Doesn't do much, just dispatches to the other functions in this file.
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
    // rmt_DestroyGlobalInstance(rmt); // crashes for some reason
    return 0;
}