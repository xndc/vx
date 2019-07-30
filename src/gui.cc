extern "C" {
    #include "./gui.h"
}
#include <glad/glad.h>
#include <glfw/glfw3.h>
#include <imgui.h>
#include <examples/imgui_impl_opengl3.h>
#include <examples/imgui_impl_glfw.h>

extern "C" bool UI_ShowDebugOverlay = false;
extern "C" int UI_DebugOverlayKey = GLFW_KEY_GRAVE_ACCENT;

extern "C" void InitGUI (GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.Fonts->AddFontFromFileTTF("fonts/Roboto-Medium.ttf", 16.0f);
    // NOTE: the default is 150, which doesn't work on macOS
    ImGui_ImplOpenGL3_Init("#version 140");
    ImGui_ImplGlfw_InitForOpenGL(window, true);
}

extern "C" void DrawGUI (GLFWwindow* window) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    static int debugOverlayKeyState = GLFW_RELEASE;
    if (glfwGetKey(window, GLFW_KEY_GRAVE_ACCENT) == GLFW_PRESS &&
        debugOverlayKeyState == GLFW_RELEASE) {
        UI_ShowDebugOverlay = !UI_ShowDebugOverlay;
    }
    debugOverlayKeyState = glfwGetKey(window, GLFW_KEY_GRAVE_ACCENT);

    if (UI_ShowDebugOverlay) {
        ImGui::ShowDemoWindow();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}