#include "gui.h"
#include <glad/glad.h>
#include <glfw/glfw3.h>
#include <imgui.h>
#include <examples/imgui_impl_opengl3.h>
#include <examples/imgui_impl_glfw.h>
#include <flib/ringbuffer.h>

static bool UI_ShowDebugOverlay = false;
static int UI_DebugOverlayKey = GLFW_KEY_GRAVE_ACCENT;

#define X(name, path, size) VX_EXPORT ImFont* name;
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
    ImGui::PushFont(FONT_ROBOTO_MEDIUM_32);
    ImVec2 textsize1 = ImGui::CalcTextSize(text1);
    ImGui::PopFont();
    ImGui::PushFont(FONT_ROBOTO_MEDIUM_16);
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
    ImGui::PushFont(FONT_ROBOTO_MEDIUM_32);
    ImGui::TextColored(ImColor(fgr, fgg, fgb), text1);
    ImGui::PopFont();
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(x2, y2));
    ImGui::SetNextWindowSize(ImVec2(w2, h2));
    ImGui::Begin("Loading details", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoBackground);
    ImGui::PushFont(FONT_ROBOTO_MEDIUM_16);
    ImGui::TextColored(ImColor(fgr, fgg, fgb), text2);
    ImGui::PopFont();
    ImGui::End();

    GUI_Render();
    glfwSwapBuffers(window);
    glfwPollEvents();
}

VX_EXPORT void GUI_DrawLoadingText (const char* text, float r, float g, float b) {
    // NOTE: Modelled after ShowExampleAppSimpleOverlay in imgui_demo.cpp
    ImGuiIO& io = ImGui::GetIO();
    ImGui::PushFont(FONT_ROBOTO_MEDIUM_32);
    ImVec2 text_size = ImGui::CalcTextSize(text);
    float x = (io.DisplaySize.x / 2.0f) - (text_size.x / 2.0f);
    float y = (io.DisplaySize.y / 2.0f) - (text_size.y / 2.0f);
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::Begin("Loading", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoBackground);
    ImGui::TextColored(ImColor(r, g, b), text);
    ImGui::End();
    ImGui::PopFont();
}

VX_EXPORT void GUI_DrawDebugOverlay (GLFWwindow* window) {
    ImGui::ShowDemoWindow();
}

VX_EXPORT void GUI_DrawStatistics (GUI_Statistics* stats) {
    RingBufferDeclareStatic(lastMsMainThread, float, 100);
    RingBufferDeclareStatic(lastMsRenderThread, float, 100);

    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowBgAlpha(0.4f);
    ImGui::Begin("Statistics", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImGui::PushFont(FONT_ROBOTO_MEDIUM_16);

    static double avgMs = 0;
    if (avgMs == 0) { avgMs = stats->msFrame; }
    avgMs = (99.0 * avgMs + stats->msFrame) / 100.0;
    double avgFps = 1000.0f / avgMs;

    ImGui::PushFont(FONT_ROBOTO_BOLD_16);
    ImGui::Text("Graphics:");
    ImGui::PopFont();
    ImGui::SameLine(180); ImGui::Text("%.0lf FPS (%.02lf ms)", avgFps, avgMs);

    if (RingBufferFull(lastMsMainThread)) {
        RingBufferPop(lastMsMainThread);
    }
    RingBufferPushItem(lastMsMainThread, (float) stats->msMainThread);
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
    RingBufferPushItem(lastMsRenderThread, (float) stats->msRenderThread);
    float avgMsRenderThread = 0;
    float maxMsRenderThread = 0;
    for (ptrdiff_t i = 0; i < RingBufferUsed(lastMsRenderThread); i++) {
        avgMsRenderThread += lastMsRenderThread[i];
        if (lastMsRenderThread[i] > maxMsRenderThread) {
            maxMsRenderThread = lastMsRenderThread[i];
        }
    }
    avgMsRenderThread /= (float) RingBufferUsed(lastMsRenderThread);

    ImGui::Text("Main thread:");
    ImGui::SameLine(100); ImGui::Text("%.2lf avg ms", avgMsMainThread);
    ImGui::SameLine(200); ImGui::Text("%.2lf max ms", maxMsMainThread);

    ImGui::Text("Renderer:");
    ImGui::SameLine(100); ImGui::Text("%.2lf avg ms", avgMsRenderThread);
    ImGui::SameLine(200); ImGui::Text("%.2lf max ms", maxMsRenderThread);

    static double avgPoll = 0;
    static double avgSwap = 0;
    if (avgPoll == 0) { avgPoll = stats->msPollEvents;  }
    if (avgSwap == 0) { avgSwap = stats->msSwapBuffers; }
    avgPoll = (99.0 * avgPoll + stats->msPollEvents)  / 100.0;
    avgSwap = (99.0 * avgSwap + stats->msSwapBuffers) / 100.0;

    ImGui::Text("PollEvents: %.2lf ms", avgPoll);
    ImGui::SameLine(150); ImGui::Text("SwapBuffers: %.2lf ms", avgSwap);

    ImGui::Text("Tris: %.01fk", ((float) stats->triangles) / 1000.0f);
    ImGui::SameLine(100); ImGui::Text("Verts: %.01fk", ((float) stats->vertices) / 1000.0f);
    ImGui::SameLine(200); ImGui::Text("Draws: %ju", stats->drawcalls);

    #if 0
    ImGui::Text("Ringbuffer used: %ju", RingBufferUsed(lastMsMainThread));
    #endif

    ImGui::PopFont();
    ImGui::End();
}