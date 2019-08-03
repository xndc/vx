#include "common.h"
#include "gui.h"
#include "flib/accessor.h"
#include "flib/array.h"
#include "render/render.h"
#include <glad/glad.h>
#include <glfw/glfw3.h>

static void GlfwErrorCallback (int code, const char* error) {
    VXERROR("GLFW error %d: %s", code, error);
}

// #define GLENUM_FRAMEBUFFERSTATUS \
//     X(GL_FRAMEBUFFER_UNDEFINED)\
//     X(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)\
//     X(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)\
//     X(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER)\
//     X(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER)\
//     X(GL_FRAMEBUFFER_UNSUPPORTED)\
//     X(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE)\
//     X(GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS)

// GLenum vxglCheckFramebufferStatus (GLuint framebuffer) {
//     glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
//     GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
//     if (status != GL_FRAMEBUFFER_COMPLETE) {
//         switch (status) {
//             #define X(VAL) case VAL: \
//                 VXPANIC("Framebuffer error for %d: %s (0x%x)", framebuffer, #VAL, status); \
//                 break;
//             GLENUM_FRAMEBUFFERSTATUS
//             #undef X
//             default:
//                 VXPANIC("Framebuffer error for %d: unknown code 0x%x", framebuffer, status);
//         };
//     }
//     return status;
// }

// GLuint G_Buffer_Depth;
// GLuint G_Buffer_ColorLDR;
// GLuint G_Buffer_ColorHDR;
// GLuint G_Buffer_ColorTAA;
// GLuint G_Buffer_Normal;
// GLuint G_Buffer_Velocity;
// GLuint G_Buffer_Aux1;
// GLuint G_Buffer_Aux2;
// GLuint G_Buffer_AuxDepth1;
// GLuint G_Buffer_Shadow1;
// GLuint G_Buffer_ShadowColor1;

// GLuint G_Framebuffer_Main;
// GLuint G_Framebuffer_Shadow;

// static void CreateBuffer (GLuint* buffer, GLenum format, int w, int h) {
//     if (*buffer != 0) {
//         glDeleteTextures(1, buffer);
//     }
//     glGenTextures(1, buffer);
//     glBindTexture(GL_TEXTURE_2D, *buffer);
//     glTexStorage2D(GL_TEXTURE_2D, 1, format, w, h);
// }

// static void CreateMainFramebuffer (int w, int h) {
//     if (G_Framebuffer_Main != 0) {
//         glDeleteFramebuffers(1, &G_Framebuffer_Main);
//     }
//     glGenFramebuffers(1, &G_Framebuffer_Main);
//     glBindFramebuffer(GL_FRAMEBUFFER, G_Framebuffer_Main);

//     CreateBuffer(&G_Buffer_Depth,       GL_DEPTH32F_STENCIL8,   w, h);
//     CreateBuffer(&G_Buffer_ColorLDR,    GL_RGBA8,               w, h);
//     CreateBuffer(&G_Buffer_ColorHDR,    GL_R11F_G11F_B10F,      w, h);
//     CreateBuffer(&G_Buffer_ColorTAA,    GL_R11F_G11F_B10F,      w, h);
//     CreateBuffer(&G_Buffer_Normal,      GL_RGB16F,              w, h);
//     CreateBuffer(&G_Buffer_Velocity,    GL_RG16F,               w, h);
//     CreateBuffer(&G_Buffer_Aux1,        GL_RGBA8,               w, h);
//     CreateBuffer(&G_Buffer_Aux2,        GL_RGBA8,               w, h);
//     CreateBuffer(&G_Buffer_AuxDepth1,   GL_DEPTH_COMPONENT32F,  w, h);

//     #define TEXTURE(attach, tex) \
//         glFramebufferTexture2D(GL_FRAMEBUFFER, attach, GL_TEXTURE_2D, tex, 0);
//     TEXTURE(GL_DEPTH_STENCIL_ATTACHMENT,    G_Buffer_Depth);
//     TEXTURE(GL_COLOR_ATTACHMENT0,           G_Buffer_ColorLDR);
//     TEXTURE(GL_COLOR_ATTACHMENT1,           G_Buffer_ColorHDR);
//     TEXTURE(GL_COLOR_ATTACHMENT2,           G_Buffer_ColorTAA);
//     TEXTURE(GL_COLOR_ATTACHMENT3,           G_Buffer_Normal);
//     TEXTURE(GL_COLOR_ATTACHMENT4,           G_Buffer_Velocity);
//     TEXTURE(GL_COLOR_ATTACHMENT5,           G_Buffer_Aux1);
//     TEXTURE(GL_COLOR_ATTACHMENT6,           G_Buffer_Aux2);
//     #undef TEXTURE

//     vxglCheckFramebufferStatus(G_Framebuffer_Main);
//     VXINFO("Created G_Framebuffer_Main (%d) with size %dx%d", G_Framebuffer_Main, w, h);
// }

// void CreateShadowFramebuffer (int w, int h) {
//     if (G_Framebuffer_Shadow != 0) {
//         glDeleteFramebuffers(1, &G_Framebuffer_Shadow);
//     }
//     glGenFramebuffers(1, &G_Framebuffer_Shadow);
//     glBindFramebuffer(GL_FRAMEBUFFER, G_Framebuffer_Shadow);

//     CreateBuffer(&G_Buffer_Shadow1,         GL_DEPTH_COMPONENT32F,  w, h);
//     CreateBuffer(&G_Buffer_ShadowColor1,    GL_RGBA8,               w, h);

//     #define TEXTURE(attach, tex) \
//         glFramebufferTexture2D(GL_FRAMEBUFFER, attach, GL_TEXTURE_2D, tex, 0);
//     TEXTURE(GL_DEPTH_ATTACHMENT,    G_Buffer_Shadow1);
//     TEXTURE(GL_COLOR_ATTACHMENT0,   G_Buffer_ShadowColor1);
//     #undef TEXTURE

//     vxglCheckFramebufferStatus(G_Framebuffer_Shadow);
//     VXINFO("Created G_Framebuffer_Shadow (%d) with size %dx%d", G_Framebuffer_Shadow, w, h);
// }

// GLuint G_Program_GBuffer_BasicLit;
// GLuint G_Program_GBuffer_MetallicRoughness;
// GLuint G_Program_Light_Directional;
// GLuint G_Program_Light_Point;
// GLuint G_Program_Composite;

// void CreatePrograms() {
//     FArray* defines = FArrayAlloc(sizeof(ProgramDefine), 128, NULL, NULL);
//     #define P(prog, vs, fs) prog = GetProgram("shaders/" vs, "shaders/" fs, defines)

//     FArrayClear(defines);
//     P(G_Program_GBuffer_BasicLit, "default.vert", "default.frag");

//     FArrayClear(defines);
//     P(G_Program_GBuffer_MetallicRoughness, "default.vert", "default.frag");

//     FArrayClear(defines);
//     P(G_Program_Light_Directional, "default.vert", "default.frag");

//     FArrayClear(defines);
//     P(G_Program_Light_Point, "default.vert", "default.frag");

//     FArrayClear(defines);
//     P(G_Program_Composite, "default.vert", "default.frag");

//     FArrayFree(defines);
//     #undef P
// }

int G_WindowConfig_W = 1280;
int G_WindowConfig_H = 1024;
int G_RenderConfig_ShadowMapSize = 2048;

int main() {
    glfwSetErrorCallback(GlfwErrorCallback);
    VXCHECK(glfwInit());

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, true);
    GLFWwindow* window = glfwCreateWindow(G_WindowConfig_W, G_WindowConfig_H, "VX", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGL();

    if (glfwExtensionSupported("WGL_EXT_swap_control_tear") ||
        glfwExtensionSupported("GLX_EXT_swap_control_tear")) {
        glfwSwapInterval(-1);
    } else {
        glfwSwapInterval(1);
    }

    // Clear window to prevent white flash before loading resources:
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    InitTextureSystem();
    InitModelSystem();
    InitRenderSystem();
    InitGUI(window);

    FAccessor* acc1 = FAccessorFromFile(FACCESSOR_FLOAT32, "models/Duck/Duck0.bin", 0, 1000, 0);
    FAccessor* acc2 = FAccessorFromFile(FACCESSOR_FLOAT32, "models/Duck/Duck0.bin", 0, 1000, 0);
    FAccessor* acc3 = FAccessorFromFile(FACCESSOR_FLOAT32_MAT4, "models/Duck/Duck0.bin", 100, 800, 0);
    #define DBG(acc) \
        VXDEBUG("Accessor 0x%jx with buffer 0x%jx, offset %ju, start 0x%jx, count %ju, stride %ju", \
            acc, acc->buffer, acc->offset, FAccessorData(acc), acc->count, acc->stride);
    DBG(acc1);
    DBG(acc2);
    DBG(acc3);

    // Texture* tex = GetTexture("textures/ground0/albedo.jpg", true, true);
    // Sampler smp1 = GetColorSampler(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_REPEAT);
    // Sampler smp2 = GetColorSampler(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_REPEAT);
    // Sampler smp3 = GetColorSampler(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_REPEAT);
    // Sampler smp4 = GetColorSampler(GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR, GL_REPEAT);

    while (!glfwWindowShouldClose(window)) {
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        UpdateFramebuffers(w, h, G_RenderConfig_ShadowMapSize);
        glViewport(0, 0, w, h);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FB_MAIN);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glClearColor(0.3f, 0.4f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        DrawGUI(window);
        StartRenderPass("Test");
        SetRenderProgram(VSH_DEFAULT, FSH_DEFAULT);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, FB_MAIN);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    return 0;
}