#include "gui.h"
#include "main.h"
#include "scene/core.h"
#include <glad/glad.h>
#include <glfw/glfw3.h>
#include <imgui.h>
#include <examples/imgui_impl_opengl3.h>
#include <examples/imgui_impl_glfw.h>
#include <flib/ringbuffer.h>

static bool UI_ShowDebugOverlay = false;
static int UI_DebugOverlayKey = GLFW_KEY_GRAVE_ACCENT;

#define X(name, path, size) ImFont* name;
XM_ASSETS_FONTS
#undef X

VX_EXPORT void GUI_Init (GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // NOTE: first font declared in XM_ASSETS_FONTS is the default
    const ImWchar ranges[] = {
        0x0020, 0x024F, // Basic Latin + Latin-1 + Latin Extended-A + Latin Extended-B
        0x0370, 0x06FF, // Greek, Cyrillic, Armenian, Hebrew, Arabic
        0x1E00, 0x20CF, // Latin Additional, Greek Extended, Punctuation, Superscripts and Subscripts, Currency
        0
    };
    #define X(name, path, size) name = io.Fonts->AddFontFromFileTTF(path, size, NULL, ranges);
    XM_ASSETS_FONTS
    #undef X
    io.Fonts->Build();

    ImGuiStyle& s = ImGui::GetStyle();
    s.FramePadding.y = 2;

    // NOTE: the default is 150, which doesn't work on macOS
    ImGui_ImplOpenGL3_Init("#version 140");
    ImGui_ImplGlfw_InitForOpenGL(window, true);
}

VX_EXPORT void GUI_StartFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

VX_EXPORT void GUI_Render() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// Returns true if ImGui received the current frame's mouse or keyboard inputs.
// The engine should avoid doing anything with them in this case.
VX_EXPORT bool GUI_InterfaceWantsInput() {
    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureKeyboard || io.WantCaptureMouse || io.WantTextInput;
}

VX_EXPORT void GUI_RenderLoadingFrame (GLFWwindow* window,
    const char* text1, const char* text2,
    float bgr, float bgg, float bgb,
    float fgr, float fgg, float fgb)
{
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glViewport(0, 0, w, h);
    glClearColor(bgr, bgg, bgb, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    GUI_StartFrame();

    ImGuiIO& io = ImGui::GetIO();
    ImGui::PushFont(FONT_DEFAULT_LARGE);
    ImVec2 textsize1 = ImGui::CalcTextSize(text1);
    ImGui::PopFont();
    ImGui::PushFont(FONT_DEFAULT);
    ImVec2 textsize2 = ImGui::CalcTextSize(text2);
    ImGui::PopFont();
    float w1 = textsize1.x + 12.0f;
    float h1 = textsize1.y + 12.0f;
    float w2 = textsize2.x + 12.0f;
    float h2 = textsize2.y + 12.0f;
    float x1 = (io.DisplaySize.x / 2.0f) - (w1 / 2.0f);
    float y1 = (io.DisplaySize.y / 2.0f) - (h1 / 2.0f) - textsize2.y + 8.0f;
    float x2 = (io.DisplaySize.x / 2.0f) - (w2 / 2.0f);
    float y2 = (io.DisplaySize.y / 2.0f) - (h2 / 2.0f) + textsize1.y - 8.0f;

    ImGui::SetNextWindowPos(ImVec2(x1, y1));
    ImGui::SetNextWindowSize(ImVec2(w1, h1));
    ImGui::Begin("Loading", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoBackground);
    ImGui::PushFont(FONT_DEFAULT_LARGE);
    ImGui::TextColored(ImColor(fgr, fgg, fgb), text1);
    ImGui::PopFont();
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(x2, y2));
    ImGui::SetNextWindowSize(ImVec2(w2, h2));
    ImGui::Begin("Loading details", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoBackground);
    ImGui::PushFont(FONT_DEFAULT);
    ImGui::TextColored(ImColor(fgr, fgg, fgb), text2);
    ImGui::PopFont();
    ImGui::End();

    GUI_Render();
    glfwSwapBuffers(window);
    glfwPollEvents();
}

VX_EXPORT void GUI_DrawStatistics (vxFrame* frame) {
    // NOTE: The point of keeping track of the frame number here is to avoid avgMs being computed
    //   by averaging N zeroes and one valid frame number. Since avgMs is static, we need the frame
    //   number to be static as well and can't rely on frame->n.
    static int avgFrames = -1;
    static double avgMs = 0;
    avgFrames++;

    const int avgInterval = 30;

    ImGui::SetNextWindowSize(ImVec2(300, 0));
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowBgAlpha(0.4f);
    ImGui::Begin("Statistics", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImGui::PushFont(FONT_DEFAULT);

    if (avgFrames < avgInterval) { avgMs = (frame->dt * 1000.0f); }
    avgMs = (float(avgInterval - 1) * avgMs + (frame->dt * 1000.0f)) / avgInterval;
    double avgFps = 1000.0f / avgMs;

    ImGui::PushFont(FONT_DEFAULT_BOLD);
    ImGui::Text("Graphics:");
    ImGui::PopFont();
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 120);
    ImGui::Text("%.0lf FPS (%.02lf ms)", avgFps, avgMs);

    // This does three main things:
    // * Statically declares a ringbuffer called [name].
    // * Pushes [thisFrameMs] to the ringbuffer on every invocation of this macro.
    // * Computes the average and maximum values for the entire ringbuffer.
    #define AVG_MAX_RINGBUFFER(name, interval, avgMsVar, maxMsVar, thisFrameMs) \
        RingBufferDeclareStatic(name, float, interval); \
        float avgMsVar = 0; \
        float maxMsVar = 0; \
        if (RingBufferFull(name)) { RingBufferPop(name); } \
        RingBufferPushItem(name, thisFrameMs); \
        for (ptrdiff_t i = 0; i < RingBufferUsed(name); i++) { \
            avgMsVar += name[i]; \
            if (name[i] > maxMsVar) { maxMsVar = name[i]; } \
        } \
        avgMsVar /= (float) RingBufferUsed(name);

    AVG_MAX_RINGBUFFER(bufMain,   avgInterval, avgMsMain,   maxMsMain,   frame->tMain   * 1000.0f);
    AVG_MAX_RINGBUFFER(bufSubmit, avgInterval, avgMsSubmit, maxMsSubmit, frame->tSubmit * 1000.0f);
    AVG_MAX_RINGBUFFER(bufRender, avgInterval, avgMsRender, maxMsRender, frame->tRender * 1000.0f);

    ImGui::Text("Main:");
    ImGui::SameLine(100); ImGui::Text("%.2lf avg ms", avgMsMain);
    ImGui::SameLine(200); ImGui::Text("%.2lf max ms", maxMsMain);

    ImGui::Text("GPU Submit:");
    ImGui::SameLine(100); ImGui::Text("%.2lf avg ms", avgMsSubmit);
    ImGui::SameLine(200); ImGui::Text("%.2lf max ms", maxMsSubmit);

    #if RMT_USE_OPENGL == 1
    ImGui::TextColored(ImColor(200, 200, 200), "GPU Render:");
    ImGui::SameLine(100); ImGui::TextColored(ImColor(200, 200, 200), "%.2lf avg ms", avgMsRender);
    ImGui::SameLine(200); ImGui::TextColored(ImColor(200, 200, 200), "%.2lf max ms", maxMsRender);
    #else
    ImGui::Text("GPU Render:");
    ImGui::SameLine(100); ImGui::Text("%.2lf avg ms", avgMsRender);
    ImGui::SameLine(200); ImGui::Text("%.2lf max ms", maxMsRender);
    #endif

    ImGui::Text("Tris: %.01fk", ((float) frame->perfTriangles) / 1000.0f);
    ImGui::SameLine(100); ImGui::Text("Verts: %.01fk", ((float) frame->perfVertices) / 1000.0f);
    ImGui::SameLine(200); ImGui::Text("Draws: %ju", frame->perfDrawCalls);

    static double avgPoll = 0;
    static double avgSwap = 0;
    if (avgFrames < avgInterval) { avgPoll = (frame->tPoll * 1000.0f); }
    if (avgFrames < avgInterval) { avgSwap = (frame->tSwap * 1000.0f); }
    avgPoll = (float(avgInterval - 1) * avgPoll + (frame->tPoll * 1000.0f)) / avgInterval;
    avgSwap = (float(avgInterval - 1) * avgSwap + (frame->tSwap * 1000.0f)) / avgInterval;

    ImGui::Text("PollEvents: %.2lf ms", avgPoll);
    ImGui::SameLine(140); ImGui::Text("SwapBuffers: %.2lf ms", avgSwap);

    ImGui::PopFont();
    ImGui::End();
}

static void sDrawImguiDemo       (vxConfig* conf, GLFWwindow* window);
static void sDrawTonemapSettings (vxConfig* conf, GLFWwindow* window);
static void sDrawBufferViewer    (vxConfig* conf, GLFWwindow* window);
static void sDrawSceneViewer     (vxConfig* conf, GLFWwindow* window, Scene* scene);
static void sDrawConfigurator    (vxConfig* conf, GLFWwindow* window);

VX_EXPORT void GUI_DrawDebugUI (vxConfig* conf, GLFWwindow* window, Scene* scene) {
    static bool showImguiDemo       = false;
    static bool showTonemapSettings = false;
    static bool showBufferViewer    = false;
    static bool showSceneViewer     = false;
    static bool showConfigurator    = false;

    #define ToggleOnConditionOnce(boolean, cond) \
        static bool boolean ## __triggered = false; \
        if (cond) { \
            if (!(boolean ## __triggered)) { \
                boolean = !boolean; \
                boolean ## __triggered = true; \
            } \
        } else { \
            boolean ## __triggered = false; \
        }
    
    #define KeyDown(key) glfwGetKey(window, key) == GLFW_PRESS
    
    ToggleOnConditionOnce(showImguiDemo,       KeyDown(GLFW_KEY_LEFT_CONTROL) && KeyDown(GLFW_KEY_I));
    ToggleOnConditionOnce(showTonemapSettings, KeyDown(GLFW_KEY_LEFT_CONTROL) && KeyDown(GLFW_KEY_T));
    ToggleOnConditionOnce(showBufferViewer,    KeyDown(GLFW_KEY_LEFT_CONTROL) && KeyDown(GLFW_KEY_B));
    ToggleOnConditionOnce(showSceneViewer,     KeyDown(GLFW_KEY_LEFT_CONTROL) && KeyDown(GLFW_KEY_O));
    ToggleOnConditionOnce(showConfigurator,    KeyDown(GLFW_KEY_LEFT_CONTROL) && KeyDown(GLFW_KEY_P));

    if (showImguiDemo)       { sDrawImguiDemo       (conf, window); }
    if (showTonemapSettings) { sDrawTonemapSettings (conf, window); }
    if (showBufferViewer)    { sDrawBufferViewer    (conf, window); }
    if (showSceneViewer)     { sDrawSceneViewer     (conf, window, scene); }
    if (showConfigurator)    { sDrawConfigurator    (conf, window); }
}

static void sDrawImguiDemo (vxConfig* conf, GLFWwindow* window) {
    ImGui::ShowDemoWindow();
}

static void sDrawTonemapSettings (vxConfig* conf, GLFWwindow* window) {
    ImGui::Begin("Tonemapper Settings", NULL);
    ImGui::RadioButton("Linear",              &conf->tonemapMode, TONEMAP_LINEAR);
    ImGui::RadioButton("Reinhard",            &conf->tonemapMode, TONEMAP_REINHARD);
    ImGui::RadioButton("Hable (Uncharted 2)", &conf->tonemapMode, TONEMAP_HABLE);
    ImGui::RadioButton("ACES Filmic",         &conf->tonemapMode, TONEMAP_ACES);
    ImGui::SliderFloat("Exposure", &conf->tonemapExposure, 0.0f, 30.0f);
    ImGui::SliderFloat("ACES A", &conf->tonemapACESParamA, 0.0f, 3.0f);
    ImGui::SliderFloat("ACES B", &conf->tonemapACESParamB, 0.0f, 3.0f);
    ImGui::SliderFloat("ACES C", &conf->tonemapACESParamC, 0.0f, 3.0f);
    ImGui::SliderFloat("ACES D", &conf->tonemapACESParamD, 0.0f, 3.0f);
    ImGui::SliderFloat("ACES E", &conf->tonemapACESParamE, 0.0f, 3.0f);
    ImGui::End();
}

static void sDrawBufferViewer (vxConfig* conf, GLFWwindow* window) {
    ImGui::Begin("Buffer Viewer", NULL);
    ImGui::RadioButton("Final Output",     &conf->debugVisMode, DEBUG_VIS_NONE);
    ImGui::RadioButton("GBuffer Color",    &conf->debugVisMode, DEBUG_VIS_GBUF_COLOR);
    ImGui::RadioButton("GBuffer Normal",   &conf->debugVisMode, DEBUG_VIS_GBUF_NORMAL);
    ImGui::RadioButton("GBuffer O/R/M",    &conf->debugVisMode, DEBUG_VIS_GBUF_ORM);
    ImGui::RadioButton("GBuffer Velocity", &conf->debugVisMode, DEBUG_VIS_GBUF_VELOCITY);
    ImGui::RadioButton("World Position",   &conf->debugVisMode, DEBUG_VIS_WORLDPOS);
    ImGui::RadioButton("Depth (Raw)",      &conf->debugVisMode, DEBUG_VIS_DEPTH_RAW);
    ImGui::RadioButton("Depth (Linear)",   &conf->debugVisMode, DEBUG_VIS_DEPTH_LINEAR);
    ImGui::RadioButton("Shadow Map",       &conf->debugVisMode, DEBUG_VIS_SHADOWMAP);
    ImGui::End();
}

static const int SHOW_MODELS = 1 << 0;
static const int SHOW_POINT_LIGHTS = 1 << 1;
static const int SHOW_OTHER_LIGHTS = 1 << 2;

static void sDrawSceneViewerObjectList (vxConfig* conf, Scene* scene, int show) {
    const int sliderMarginLeft = 30;
    for (int i = 0; i < (int) scene->size; i++) {
        GameObject* obj = &scene->objects[i];
        ImGui::PushID(i);

        #define PARENTED_VIEW() \
            if (obj->parent != NULL) { \
                ImGui::Spacing(); ImGui::SameLine(sliderMarginLeft); \
                int index = -1; \
                for (int ip = 0; ip < scene->size; ip++) { \
                    if (&scene->objects[ip] == obj->parent) { \
                        index = ip; \
                    } \
                } \
                ImGui::TextColored(ImColor(200, 200, 200), "Parented to object %d.", index); \
                ImGui::SameLine(284); if (ImGui::Button("Unparent")) { \
                    obj->parent = NULL; \
                    obj->needsUpdate = true; \
                    mat4 rot; \
                    glm_decompose(obj->worldMatrix, obj->localPosition, rot, obj->localScale); \
                    glm_mat4_quat(rot, obj->localRotation); \
                } \
            }
        
        if (obj->type == GAMEOBJECT_MODEL && (show & SHOW_MODELS)) {
            ImGui::Text("%d: Model %s", i, obj->model.model->name);
            ImGui::SameLine(300); if (ImGui::Button("Delete")) {
                DeleteObjectFromScene(scene, obj);
            }
            ImGui::Spacing(); ImGui::SameLine(sliderMarginLeft);
            ImGui::DragFloat3("Position", obj->localPosition, 0.05f, -100.0f, 100.0f);
            ImGui::Spacing(); ImGui::SameLine(sliderMarginLeft);
            ImGui::DragFloat3("Scale", obj->localScale, 0.01f, 0.01f, 20.0f);
            PARENTED_VIEW();
            ImGui::Separator();
        }
        
        if (obj->type == GAMEOBJECT_POINT_LIGHT && (show & SHOW_POINT_LIGHTS)) {
            ImGui::Text("%d: Point Light", i);
            ImGui::SameLine(150); ImGui::Checkbox("Intensity Only", &obj->pointLight.editorIntensityMode);
            ImGui::SameLine(300); if (ImGui::Button("Delete")) {
                DeleteObjectFromScene(scene, obj);
            }
            if (obj->pointLight.editorIntensityMode) {
                ImGui::Spacing(); ImGui::SameLine(sliderMarginLeft);
                float c = glm_vec3_max(obj->pointLight.color);
                ImGui::DragFloat("Intensity", &c, 0.01f, 0.0f, 20.0f);
                glm_vec3_broadcast(c, obj->pointLight.color);
            } else {
                ImGui::Spacing(); ImGui::SameLine(sliderMarginLeft);
                ImGui::DragFloat3("Color", obj->pointLight.color, 0.01f, 0.0f, 20.0f);
            }
            ImGui::Spacing(); ImGui::SameLine(sliderMarginLeft);
            ImGui::DragFloat3("Position", obj->localPosition, 0.05f, -100.0f, 100.0f);
            PARENTED_VIEW();
            ImGui::Separator();
        }
        
        if (obj->type == GAMEOBJECT_DIRECTIONAL_LIGHT && (show & SHOW_OTHER_LIGHTS)) {
            ImGui::Text("%d: Directional Light", i);
            ImGui::SameLine(150); ImGui::Checkbox("Intensity Only", &obj->directionalLight.editorIntensityMode);
            if (obj->directionalLight.editorIntensityMode) {
                ImGui::Spacing(); ImGui::SameLine(sliderMarginLeft);
                float c = glm_vec3_max(obj->directionalLight.color);
                ImGui::DragFloat("Intensity", &c, 0.01f, 0.0f, 20.0f);
                glm_vec3_broadcast(c, obj->directionalLight.color);
            } else {
                ImGui::Spacing(); ImGui::SameLine(sliderMarginLeft);
                ImGui::DragFloat3("Color", obj->directionalLight.color, 0.01f, 0.0f, 20.0f);
            }
            ImGui::Spacing(); ImGui::SameLine(sliderMarginLeft);
            ImGui::DragFloat3("Position", obj->localPosition, 0.01f, -2.0f, 2.0f);
            PARENTED_VIEW();
            ImGui::Separator();
        }
        
        if (obj->type == GAMEOBJECT_LIGHT_PROBE && (show & SHOW_OTHER_LIGHTS)) {
            ImGui::Text("%d: Light Probe", i);
            ImGui::SameLine(150); ImGui::Checkbox("Intensity Only", &obj->lightProbe.editorIntensityMode);
            if (obj->lightProbe.editorIntensityMode) {
                float xc[2] = { glm_vec3_max(obj->lightProbe.colorXp), glm_vec3_max(obj->lightProbe.colorXn) };
                float yc[2] = { glm_vec3_max(obj->lightProbe.colorYp), glm_vec3_max(obj->lightProbe.colorYn) };
                float zc[2] = { glm_vec3_max(obj->lightProbe.colorZp), glm_vec3_max(obj->lightProbe.colorZn) };
                ImGui::Spacing(); ImGui::SameLine(sliderMarginLeft);
                ImGui::DragFloat2("X+/X-", xc, 0.0025f, 0.0f, 5.0f);
                ImGui::Spacing(); ImGui::SameLine(sliderMarginLeft);
                ImGui::DragFloat2("Y+/Y-", yc, 0.0025f, 0.0f, 5.0f);
                ImGui::Spacing(); ImGui::SameLine(sliderMarginLeft);
                ImGui::DragFloat2("Z+/Z-", zc, 0.0025f, 0.0f, 5.0f);
                glm_vec3_broadcast(xc[0], obj->lightProbe.colorXp);
                glm_vec3_broadcast(xc[1], obj->lightProbe.colorXn);
                glm_vec3_broadcast(yc[0], obj->lightProbe.colorYp);
                glm_vec3_broadcast(yc[1], obj->lightProbe.colorYn);
                glm_vec3_broadcast(zc[0], obj->lightProbe.colorZp);
                glm_vec3_broadcast(zc[1], obj->lightProbe.colorZn);
            } else {
                ImGui::Spacing(); ImGui::SameLine(sliderMarginLeft);
                ImGui::DragFloat3("X+ Color", obj->lightProbe.colorXp, 0.0025f, 0.0f, 5.0f);
                ImGui::Spacing(); ImGui::SameLine(sliderMarginLeft);
                ImGui::DragFloat3("X- Color", obj->lightProbe.colorXn, 0.0025f, 0.0f, 5.0f);
                ImGui::Spacing(); ImGui::SameLine(sliderMarginLeft);
                ImGui::DragFloat3("Y+ Color", obj->lightProbe.colorYp, 0.0025f, 0.0f, 5.0f);
                ImGui::Spacing(); ImGui::SameLine(sliderMarginLeft);
                ImGui::DragFloat3("Y- Color", obj->lightProbe.colorYn, 0.0025f, 0.0f, 5.0f);
                ImGui::Spacing(); ImGui::SameLine(sliderMarginLeft);
                ImGui::DragFloat3("Z+ Color", obj->lightProbe.colorZp, 0.0025f, 0.0f, 5.0f);
                ImGui::Spacing(); ImGui::SameLine(sliderMarginLeft);
                ImGui::DragFloat3("Z- Color", obj->lightProbe.colorZn, 0.0025f, 0.0f, 5.0f);
            }
            #if 0 // not needed yet
            ImGui::Spacing(); ImGui::SameLine(sliderMarginLeft);
            ImGui::DragFloat3("Position", obj->localPosition, 0.05f, -100.0f, 100.0f);
            #endif
            PARENTED_VIEW();
            ImGui::Separator();
        }
        
        #undef PARENTED_VIEW
        ImGui::PopID();
    }
}

static void sDrawSceneViewer (vxConfig* conf, GLFWwindow* window, Scene* scene) {
    ImGui::Begin("Scene Viewer", NULL, ImGuiWindowFlags_MenuBar);
    
    ImGui::BeginMenuBar();
    if (ImGui::BeginMenu("Add Object")) {
        for (size_t i = 0; i < ModelCount; i++) {
            if (ImGui::MenuItem(Models[i]->name, NULL)) {
                GameObject* obj = AddObject(scene, NULL, GAMEOBJECT_MODEL);
                obj->model.model = Models[i];
            }
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s", Models[i]->sourceFilePath);
                ImGui::EndTooltip();
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Point Light")) {
            GameObject* obj = AddObject(scene, NULL, GAMEOBJECT_POINT_LIGHT);
            glm_vec3_broadcast(1.0f, obj->pointLight.color);
        }
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
    
    ImGui::BeginTabBar("Object Types Tab Bar");
    if (ImGui::BeginTabItem("All Objects")) {
        sDrawSceneViewerObjectList(conf, scene, SHOW_MODELS | SHOW_POINT_LIGHTS | SHOW_OTHER_LIGHTS);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Models")) {
        sDrawSceneViewerObjectList(conf, scene, SHOW_MODELS);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Point Lights")) {
        sDrawSceneViewerObjectList(conf, scene, SHOW_POINT_LIGHTS);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Other Lights")) {
        sDrawSceneViewerObjectList(conf, scene, SHOW_OTHER_LIGHTS);
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
    ImGui::End();
}

static void sDrawConfigurator (vxConfig* conf, GLFWwindow* window) {
    ImGui::Begin("Engine Configuration");
    ImGui::Checkbox("Pause on focus loss", &conf->pauseOnFocusLoss);

    ImGui::Checkbox("Enable TAA", &conf->enableTAA);
    ImGui::SliderFloat("TAA sample offset multiplier", &conf->taaSampleOffsetMul, 0.0, 4.0);
    ImGui::SliderFloat("TAA neighbourhood clamping distance", &conf->taaClampSampleDist, 0.0, 4.0);
    ImGui::SliderFloat("TAA feedback factor", &conf->taaFeedbackFactor, 0.0, 1.0);
    ImGui::SliderFloat("Sharpen filter strength", &conf->sharpenStrength, 0.0, 0.2);

    ImGui::SliderInt("VSync", &conf->swapInterval, -1, 3, "swap interval %d");
    ImGui::InputInt("Environment map size", &conf->envmapSize, 128, 128);

    ImGui::InputInt("Shadow map size", &conf->shadowSize, 256, 256);
    conf->shadowSize = vxClamp(conf->shadowSize, 32, 8192); // framebuffer size limit on most machines
    ImGui::DragFloat("Shadow zoom", &conf->camShadow.zoom, 5.0f, 1.0f, 1000.0f);
    ImGui::DragFloat("Shadow near plane", &conf->camShadow.near, 10.0f, -1000.0f, +1000.0f);
    ImGui::DragFloat("Shadow far plane", &conf->camShadow.far, 10.0f, -1000.0f, +1000.0f);
    ImGui::DragFloat("Shadow min bias", &conf->shadowBiasMin, 0.000001f, 0.0f, 0.1f, "%.6f");
    ImGui::DragFloat("Shadow max bias", &conf->shadowBiasMax, 0.000001f, 0.0f, 0.1f, "%.6f");
    ImGui::SliderInt2("Shadow PCF taps", &conf->shadowPcfTapsX /* will get both X and Y */, 1, 5);
    ImGui::Checkbox("Shadow TAA", &conf->shadowTAA);
    ImGui::SameLine(200);
    ImGui::Checkbox("Noisy sampling", &conf->shadowNoise);
    
    ImGui::Checkbox("Clear GBuffer on redraw", &conf->clearColorBuffers);
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Clears all color buffers at the start of the frame. By default, only depth buffers are cleared.");
        ImGui::Text("This is mostly useful when debugging under RenderDoc.");
        ImGui::EndTooltip();
    }

    ImGui::Checkbox("Enable clip control", &conf->gpuSupportsClipControl);
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Clip control allows mapping depth to the correct [0,1] range rather than [0.5,1].");
        ImGui::Text("Disabling this will significantly increase Z-fighting at medium to large distances.");
        ImGui::EndTooltip();
    }
    ImGui::End();
}