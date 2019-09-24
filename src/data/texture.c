#include "texture.h"
#include "main.h"
#include "render/render.h"

#include <glad/glad.h>
#include <glfw/glfw3.h>
#include <stb_image.h>

#define X(name, type, mips, path) GLuint name = 0;
XM_ASSETS_TEXTURES
#undef X

#define X(name, format) GLuint name = 0;
XM_RENDERTARGETS_SCREEN
XM_RENDERTARGETS_SHADOW
XM_RENDERTARGETS_ENVMAP
#undef X

#define BEGIN(name) GLuint name = 0;
#define DEPTH(point, name)
#define COLOR(point, name)
#define END(name)
XM_FRAMEBUFFERS
#undef BEGIN
#undef DEPTH
#undef COLOR
#undef END

GLuint VXGL_SAMPLER [VXGL_SAMPLER_COUNT];

// GLuint ENVMAP_BASE = 0;
// GLuint ENVMAP_CUBE = 0;

GLuint TEX_WHITE_1x1;
GLuint SMP_NEAREST;
GLuint SMP_NEAREST_REPEAT;
GLuint SMP_LINEAR;

// Initializes the texture system. Should only be run once.
void InitTextureSystem() {
    glGenTextures(1, &TEX_WHITE_1x1);
    glBindTexture(GL_TEXTURE_2D, TEX_WHITE_1x1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLubyte[]){ 255, 255, 255, 255 });
    glGenSamplers(1, &SMP_NEAREST);
    glSamplerParameteri(SMP_NEAREST, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(SMP_NEAREST, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(SMP_NEAREST, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(SMP_NEAREST, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(SMP_NEAREST, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenSamplers(1, &SMP_NEAREST_REPEAT);
    glSamplerParameteri(SMP_NEAREST_REPEAT, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glSamplerParameteri(SMP_NEAREST_REPEAT, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glSamplerParameteri(SMP_NEAREST_REPEAT, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glSamplerParameteri(SMP_NEAREST_REPEAT, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(SMP_NEAREST_REPEAT, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenSamplers(1, &SMP_LINEAR);
    glSamplerParameteri(SMP_LINEAR, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(SMP_LINEAR, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(SMP_LINEAR, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(SMP_LINEAR, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(SMP_LINEAR, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenSamplers(VXGL_SAMPLER_COUNT, VXGL_SAMPLER);
}

// Loads all textures from disk. Can be run multiple times.
void LoadTextures() {
    // Regular textures:
    #define X(name, type, mips, path) name = LoadTextureFromDisk(path, mips);
    XM_ASSETS_TEXTURES
    #undef X

#if 0
    // IBL environment map:
    // https://learnopengl.com/PBR/IBL/Diffuse-irradiance
    stbi_set_flip_vertically_on_load(true);
    int w, h, c;
    float *pixels = stbi_loadf("textures/skybox0/GCanyon_C_YumaPoint_3k.hdr", &w, &h, &c, 0);
    vxCheck(pixels);
    glGenTextures(1, &ENVMAP_BASE);
    glBindTexture(GL_TEXTURE_2D, ENVMAP_BASE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, pixels);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Preallocate texture for cubemap:
    glGenTextures(1, &ENVMAP_CUBE);
    glBindTexture(GL_TEXTURE_CUBE_MAP, ENVMAP_CUBE);
    for (int i = 0; i < 6; i++) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, 0);
    }
    // glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Generate cubemap:
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    SetRenderProgram(VSH_FULLSCREEN_PASS, FSH_GEN_CUBEMAP);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ENVMAP_BASE);
    glBindTexture(GL_TEXTURE_CUBE_MAP, ENVMAP_CUBE);
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X, ENVMAP_CUBE, 0);
        mat4 m;
        glm_rotate_make(m, 0, (vec3){0, 1, 0});
        glUniformMatrix4fv(UNIF_ENVMAP_DIRECTION, 1, false, (float*) m);
        RunFullscreenPass(512, 512, 0, 0);
    }
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_NEGATIVE_X, ENVMAP_CUBE, 0);
        mat4 m;
        glm_rotate_make(m, vxRadians(180), (vec3){0, 1, 0});
        glUniformMatrix4fv(UNIF_ENVMAP_DIRECTION, 1, false, (float*) m);
        RunFullscreenPass(512, 512, 0, 0);
    }
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_Y, ENVMAP_CUBE, 0);
        mat4 m;
        glm_rotate_make(m, vxRadians(-90), (vec3){0, 0, 1});
        glUniformMatrix4fv(UNIF_ENVMAP_DIRECTION, 1, false, (float*) m);
        RunFullscreenPass(512, 512, 0, 0);
    }
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, ENVMAP_CUBE, 0);
        mat4 m;
        glm_rotate_make(m, vxRadians(90), (vec3){0, 0, 1});
        glUniformMatrix4fv(UNIF_ENVMAP_DIRECTION, 1, false, (float*) m);
        RunFullscreenPass(512, 512, 0, 0);
    }
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_Z, ENVMAP_CUBE, 0);
        mat4 m;
        glm_rotate_make(m, vxRadians(90), (vec3){0, 1, 0});
        glUniformMatrix4fv(UNIF_ENVMAP_DIRECTION, 1, false, (float*) m);
        RunFullscreenPass(512, 512, 0, 0);
    }
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, ENVMAP_CUBE, 0);
        mat4 m;
        glm_rotate_make(m, vxRadians(-90), (vec3){0, 1, 0});
        glUniformMatrix4fv(UNIF_ENVMAP_DIRECTION, 1, false, (float*) m);
        RunFullscreenPass(512, 512, 0, 0);
    }
#endif
}

static void sUpdateRenderTarget (const char* name, const char* fmtname, GLuint* t, GLenum format, int w, int h) {
    vxLog("Rebuilding render target %s (%dx%d, %s)", name, w, h, fmtname);
    if (*t != 0) {
        glDeleteTextures(1, t);
    }
    glGenTextures(1, t);
    glBindTexture(GL_TEXTURE_2D, *t);
    glTexStorage2D(GL_TEXTURE_2D, 1, format, w, h);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// Updates all render targets and framebuffers, if required.
// Returns a bitfield containing one UPDATED_*_TARGETS flag for each type of render target that was updated.
uint8_t UpdateRenderTargets (vxConfig* conf) {
    static int lastScreenW = 0;
    static int lastScreenH = 0;
    static int lastShadowSize = 0;
    static int lastEnvmapSize = 0;
    int screenW = conf->displayW;
    int screenH = conf->displayH;
    int shadowSize = conf->shadowSize;
    int envmapSize = conf->envmapSize;
    uint8_t updated = 0;

    if (screenW == 0 || screenH == 0) {
        // Trying to generate a 0x0 framebuffer does not go well.
        return 0;
    }

    if (screenW != lastScreenW || screenH != lastScreenH) {
        updated |= UPDATED_SCREEN_TARGETS;
        #define X(name, format) sUpdateRenderTarget(#name, #format, &name, format, screenW, screenH);
        XM_RENDERTARGETS_SCREEN
        #undef X
    }

    if (shadowSize != lastShadowSize) {
        updated |= UPDATED_SHADOW_TARGETS;
        #define X(name, format) sUpdateRenderTarget(#name, #format, &name, format, shadowSize, shadowSize);
        XM_RENDERTARGETS_SHADOW
        #undef X
    }

    if (envmapSize != lastEnvmapSize) {
        updated |= UPDATED_ENVMAP_TARGETS;
        #define X(name, format) sUpdateRenderTarget(#name, #format, &name, format, envmapSize, envmapSize);
        XM_RENDERTARGETS_ENVMAP
        #undef X
    }

    if (updated) {
        GLenum status;
        #define BEGIN(name) \
            if (name != 0) { glDeleteFramebuffers(1, &name); } \
            glGenFramebuffers(1, &name); \
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, name);
        #define DEPTH(point, name) \
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, point, GL_TEXTURE_2D, name, 0);
        #define COLOR(point, name) \
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, point, GL_TEXTURE_2D, name, 0);
        #define END(name) \
            status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER); \
            if (status != GL_FRAMEBUFFER_COMPLETE) { \
                vxPanic("Couldn't create framebuffer " #name ": error %d", status); \
            }
        XM_FRAMEBUFFERS
        #undef BEGIN
        #undef DEPTH
        #undef COLOR
        #undef END
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }

    lastScreenW = screenW;
    lastScreenH = screenH;
    lastShadowSize = shadowSize;
    lastEnvmapSize = envmapSize;
    return updated;
}

// Loads a texture from disk and uploads it to the GPU. Returns its OpenGL ID.
GLuint LoadTextureFromDisk (const char* path, bool mips) {
    double tStart = glfwGetTime();
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Generate hash of texture filename and mtime:
    uint64_t mtime = vxGetFileMtime(path);
    vxCheckMsg(mtime != 0, "Failed to load %s: file not found", path);
    size_t hash = stbds_hash_string((char*) path, VX_SEED);
    hash ^= stbds_hash_bytes(&mtime, sizeof(mtime), VX_SEED);

    // Look for cached texture:
    static char cachedTexturePath [128];
    stbsp_snprintf(cachedTexturePath, 128, "userdata/texturecache/%jx.dat", hash);
    if (vxGetFileMtime(cachedTexturePath) != 0) {
        size_t size;
        char* data = vxReadFile(cachedTexturePath, "rb", &size);
        uint32_t w = ((uint32_t*) data)[0];
        uint32_t h = ((uint32_t*) data)[1];
        uint32_t c = ((uint32_t*) data)[2]; // channels
        uint32_t l = ((uint32_t*) data)[3]; // mip levels
        size_t idata = 4 * sizeof(uint32_t);
        GLenum internalformat, format;
        switch (c) {
            case 1: { internalformat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;  format = GL_RED;  break; }
            case 2: { internalformat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;  format = GL_RG;   break; }
            case 3: { internalformat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;  format = GL_RGB;  break; }
            case 4: { internalformat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; format = GL_RGBA; break; }
            default: { vxPanic("Unknown channel count %d for %s (%s)", c, path, cachedTexturePath); }
        }
        int levelw = w;
        int levelh = h;
        for (int ilevel = 0; ilevel < (int) l; ilevel++) {
            int levelsize = *(int*)(&data[idata]);
            idata += sizeof(int);
            // if ((size - idata) < levelw * levelh * c) {
            //     vxPanic("%s has wrong size %ju for parameters %ux%ux%ux%u", size, w, h, c, l);
            // }
            char* leveldata = data + idata;
            // idata += levelw * levelh * c;
            idata += levelsize;
            // glTexImage2D(GL_TEXTURE_2D, ilevel, internalformat, levelw, levelh, 0, format,
            //     GL_UNSIGNED_BYTE, leveldata);
            glCompressedTexImage2D(GL_TEXTURE_2D, ilevel, internalformat, levelw, levelh, 0, (GLsizei) levelsize,
                leveldata);
            levelw /= 2;
            levelh /= 2;
        }
        free(data);
        double t = (glfwGetTime() - tStart) * 1000.0;
        vxLog("Read from cache: %s (FBO %u, %ux%ux%ux%u, %.02lf ms)", path, texture, w, h, c, l, t);
        return texture;
    }

    // Read from disk:
    int w, h, c;
    void* image = stbi_load(path, &w, &h, &c, 0);
    if (!image) {
        vxPanic("Failed to load %s: %s", path, stbi_failure_reason());
    }

    // Compute mip level count (no way to query it):
    // https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_texture_non_power_of_two.txt
    uint32_t l = 1;
    if (mips) {
        l += (uint32_t) floor(log2(vxMax(vxMax(w, h), 2)));
    }

    // Upload:
    GLenum internalformat, format;
    switch (c) {
        case 1: { internalformat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;  format = GL_RED;  break; }
        case 2: { internalformat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;  format = GL_RG;   break; }
        case 3: { internalformat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;  format = GL_RGB;  break; }
        case 4: { internalformat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; format = GL_RGBA; break; }
        default: { vxPanic("Unknown channel count %d for %s (%s)", c, path, cachedTexturePath); }
    }
    glTexImage2D(GL_TEXTURE_2D, 0, internalformat, w, h, 0, format, GL_UNSIGNED_BYTE, image);
    if (mips) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    free(image);

    // Cache:
    vxCreateDirectory("userdata");
    vxCreateDirectory("userdata/texturecache");
    FILE* cachedTextureFile = fopen(cachedTexturePath, "wb");
    if (cachedTextureFile == 0) {
        vxLog("Warning: can't open %s for writing: %s", cachedTexturePath, strerror(errno));
    } else {
        uint32_t info[] = {(uint32_t) w, (uint32_t) h, (uint32_t) c, l};
        fwrite(info, sizeof(uint32_t), 4, cachedTextureFile);
        for (int ilevel = 0; ilevel < (int) l; ilevel++) {
            // FIXME: the docs for glGetTexImage mention GL_PACK_ALIGNMENT - what's that?
            GLint levelw, levelh, compressedSize;
            glGetTexLevelParameteriv(GL_TEXTURE_2D, ilevel, GL_TEXTURE_WIDTH,  &levelw);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, ilevel, GL_TEXTURE_HEIGHT, &levelh);
            // void* pixels = malloc((levelw + 1) * (levelh + 1) * c);
            // glGetTexImage(GL_TEXTURE_2D, ilevel, format, GL_UNSIGNED_BYTE, pixels);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, ilevel, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &compressedSize);
            void* pixels = malloc(compressedSize);
            glGetCompressedTexImage(GL_TEXTURE_2D, ilevel, pixels);
            // fwrite(pixels, 1, levelw * levelh * c, cachedTextureFile);
            fwrite(&compressedSize, sizeof(compressedSize), 1, cachedTextureFile);
            fwrite(pixels, 1, compressedSize, cachedTextureFile);
            free(pixels);
        }
        fclose(cachedTextureFile);
    }

    double t = (glfwGetTime() - tStart) * 1000.0;
    vxLog("Read from disk: %s (FBO %u, %dx%dx%dx%u, %.02lf ms)", path, texture, w, h, c, l, t);

    return texture;
}
