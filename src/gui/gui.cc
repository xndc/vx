#include "gui.h"
#include "main.h"
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

    const int avgInterval = 10;
    RingBufferDeclareStatic(lastMsMainThread,   float, avgInterval);
    RingBufferDeclareStatic(lastMsRenderThread, float, avgInterval);

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

    if (RingBufferFull(lastMsMainThread)) {
        RingBufferPop(lastMsMainThread);
    }
    RingBufferPushItem(lastMsMainThread, frame->tMain * 1000.0f);
    float avgMsMainThread = 0;
    float maxMsMainThread = 0;
    for (ptrdiff_t i = 0; i < RingBufferUsed(lastMsMainThread); i++) {
        avgMsMainThread += lastMsMainThread[i];
        if (lastMsMainThread[i] > maxMsMainThread) {
            maxMsMainThread = lastMsMainThread[i];
        }
    }
    avgMsMainThread /= (float) RingBufferUsed(lastMsMainThread);

    if (RingBufferFull(lastMsRenderThread)) {
        RingBufferPop(lastMsRenderThread);
    }
    RingBufferPushItem(lastMsRenderThread, frame->tRender * 1000.0f);
    float avgMsRenderThread = 0;
    float maxMsRenderThread = 0;
    for (ptrdiff_t i = 0; i < RingBufferUsed(lastMsRenderThread); i++) {
        avgMsRenderThread += lastMsRenderThread[i];
        if (lastMsRenderThread[i] > maxMsRenderThread) {
            maxMsRenderThread = lastMsRenderThread[i];
        }
    }
    avgMsRenderThread /= (float) RingBufferUsed(lastMsRenderThread);

    ImGui::Text("Main:");
    ImGui::SameLine(100); ImGui::Text("%.2lf avg ms", avgMsMainThread);
    ImGui::SameLine(200); ImGui::Text("%.2lf max ms", maxMsMainThread);

    ImGui::Text("Renderer:");
    ImGui::SameLine(100); ImGui::Text("%.2lf avg ms", avgMsRenderThread);
    ImGui::SameLine(200); ImGui::Text("%.2lf max ms", maxMsRenderThread);

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

    #if 0
    ImGui::Text("Ringbuffer used: %ju", RingBufferUsed(lastMsMainThread));
    #endif

    ImGui::PopFont();
    ImGui::End();
}

static void sDrawImguiDemo (vxConfig* conf, GLFWwindow* window);
static void sDrawTonemapSettings (vxConfig* conf, GLFWwindow* window);
static void sDrawBufferViewer (vxConfig* conf, GLFWwindow* window);

VX_EXPORT void GUI_DrawDebugUI (vxConfig* conf, GLFWwindow* window) {
    static bool showImguiDemo = false;
    static bool showTonemapSettings = false;
    static bool showBufferViewer = false;

    static bool kpTonemap = false;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS && glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) {
        if (!kpTonemap) {
            showTonemapSettings = !showTonemapSettings;
            kpTonemap = true;
        }
    } else {
        kpTonemap = false;
    }

    static bool kpBuffer = false;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS && glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS) {
        if (!kpBuffer) {
            showBufferViewer = !showBufferViewer;
            kpBuffer = true;
        }
    } else {
        kpBuffer = false;
    }

    if (showImguiDemo) { sDrawImguiDemo(conf, window); }
    if (showTonemapSettings) { sDrawTonemapSettings(conf, window); }
    if (showBufferViewer) { sDrawBufferViewer(conf, window); }
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
    ImGui::RadioButton("Final Output",    &conf->debugVisMode, DEBUG_VIS_NONE);
    ImGui::RadioButton("GBuffer Color",   &conf->debugVisMode, DEBUG_VIS_GBUF_COLOR);
    ImGui::RadioButton("GBuffer Normal",  &conf->debugVisMode, DEBUG_VIS_GBUF_NORMAL);
    ImGui::RadioButton("GBuffer O/R/M",   &conf->debugVisMode, DEBUG_VIS_GBUF_ORM);
    ImGui::RadioButton("World Position",  &conf->debugVisMode, DEBUG_VIS_WORLDPOS);
    ImGui::RadioButton("Depth (Raw)",     &conf->debugVisMode, DEBUG_VIS_DEPTH_RAW);
    ImGui::RadioButton("Depth (Linear)",  &conf->debugVisMode, DEBUG_VIS_DEPTH_LINEAR);
    ImGui::End();
}