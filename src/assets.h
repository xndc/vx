#pragma once

#define XM_SHADERS \
    X(GL_VERTEX_SHADER,     VSH_DEFAULT,            "shaders/default.vert") \
    X(GL_VERTEX_SHADER,     VSH_FULLSCREEN_PASS,    "shaders/fullscreen.vert") \
    X(GL_FRAGMENT_SHADER,   FSH_GBUF_MAIN,          "shaders/gbuf_main.frag") \
    X(GL_FRAGMENT_SHADER,   FSH_GBUF_LIGHTING,      "shaders/gbuf_lighting.frag") \
    X(GL_FRAGMENT_SHADER,   FSH_FX_DITHER,          "shaders/fx_dither.frag") \
    X(GL_FRAGMENT_SHADER,   FSH_FINAL,              "shaders/final.frag") \
    X(GL_FRAGMENT_SHADER,   FSH_GEN_CUBEMAP,        "shaders/gen_cubemap.frag") \
    X(GL_VERTEX_SHADER,     VSH_SKYBOX,             "shaders/skybox.vert") \
    X(GL_FRAGMENT_SHADER,   FSH_SKYBOX,             "shaders/skybox.frag") \
    X(GL_VERTEX_SHADER,     VSH_SHADOW,             "shaders/shadow.vert") \
    X(GL_FRAGMENT_SHADER,   FSH_SHADOW,             "shaders/shadow.frag") \
    X(GL_FRAGMENT_SHADER,   FSH_SHADOW_RESOLVE,     "shaders/shadow_resolve.frag") \
    X(GL_FRAGMENT_SHADER,   FSH_TAA,                "shaders/taa.frag") \

#define XM_PROGRAMS \
    X(PROG_GBUF_MAIN,       VSH_DEFAULT,         FSH_GBUF_MAIN) \
    X(PROG_SHADOW,          VSH_SHADOW,          FSH_SHADOW) \
    X(PROG_GBUF_LIGHTING,   VSH_FULLSCREEN_PASS, FSH_GBUF_LIGHTING) \
    X(PROG_SHADOW_RESOLVE,  VSH_FULLSCREEN_PASS, FSH_SHADOW_RESOLVE) \
    X(PROG_FX_DITHER,       VSH_FULLSCREEN_PASS, FSH_FX_DITHER) \
    X(PROG_FINAL,           VSH_FULLSCREEN_PASS, FSH_FINAL) \
    X(PROG_TAA,             VSH_FULLSCREEN_PASS, FSH_TAA) \

// Syntax for attributes:
// X(location global name, layout location index, GLSL name, GLTF name)
// See https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#meshes for
// GLTF attribute names.

#define XM_PROGRAM_ATTRIBUTES \
    X(ATTR_POSITION,    0, "aPosition",  "POSITION")    \
    X(ATTR_NORMAL,      1, "aNormal",    "NORMAL")      \
    X(ATTR_TANGENT,     2, "aTangent",   "TANGENT")     \
    X(ATTR_TEXCOORD0,   3, "aTexcoord0", "TEXCOORD_0")  \
    X(ATTR_TEXCOORD1,   4, "aTexcoord1", "TEXCOORD_1")  \
    X(ATTR_COLOR,       5, "aColor",     "COLOR_0")     \
    X(ATTR_JOINTS,      6, "aJoints",    "JOINTS_0")    \
    X(ATTR_WEIGHTS,     7, "aWeights",   "WEIGHTS_0")   \

// Syntax for uniforms:
// X(location global name, GLSL name)
// NOTE: the UNIF_RT uniform variable names should match the render target names defined below

#define XM_PROGRAM_UNIFORMS \
    X(UNIF_RT_DEPTH,        "gDepth") \
    X(UNIF_RT_COLOR_LDR,    "gColorLDR") \
    X(UNIF_RT_COLOR_HDR,    "gColorHDR") \
    X(UNIF_RT_NORMAL,       "gNormal") \
    X(UNIF_RT_AUX_HDR11,    "gAuxHDR11") \
    X(UNIF_RT_AUX_HDR16,    "gAuxHDR16") \
    X(UNIF_RT_AUX1,         "gAux1") \
    X(UNIF_RT_AUX2,         "gAux2") \
    X(UNIF_RT_AUX_DEPTH,    "gAuxDepth") \
    X(UNIF_RT_SHADOW_DEPTH, "gShadow") \
\
    X(UNIF_IRESOLUTION,         "iResolution") \
    X(UNIF_ITIME,               "iTime") \
    X(UNIF_IFRAME,              "iFrame") \
    X(UNIF_STIPPLE,             "uStipple") \
    X(UNIF_STIPPLE_HARD_CUTOFF, "uStippleHardCutoff") \
    X(UNIF_STIPPLE_SOFT_CUTOFF, "uStippleSoftCutoff") \
\
    X(UNIF_AMBIENT_CUBE,         "uAmbientCube") \
    X(UNIF_SUN_POSITION,         "uSunPosition") \
    X(UNIF_SUN_COLOR,            "uSunColor") \
    X(UNIF_POINTLIGHT_POSITIONS, "uPointLightPositions") \
    X(UNIF_POINTLIGHT_COLORS,    "uPointLightColors") \
\
    X(UNIF_MODEL_MATRIX,    "uModelMatrix") \
    X(UNIF_VIEW_MATRIX,     "uViewMatrix") \
    X(UNIF_PROJ_MATRIX,     "uProjMatrix") \
    X(UNIF_INV_VIEW_MATRIX, "uInvViewMatrix") \
    X(UNIF_INV_PROJ_MATRIX, "uInvProjMatrix") \
    X(UNIF_LAST_MODEL_MATRIX, "uLastModelMatrix") \
    X(UNIF_LAST_VIEW_MATRIX,  "uLastViewMatrix") \
    X(UNIF_LAST_PROJ_MATRIX,  "uLastProjMatrix") \
\
    X(UNIF_JITTER,               "uJitter") \
    X(UNIF_JITTER_LAST,          "uJitterLast") \
    X(UNIF_JITTER_MATRIX,        "uJitterMatrix") \
    X(UNIF_LAST_JITTER_MATRIX,   "uLastJitterMatrix") \
    X(UNIF_UNJITTER_MATRIX,      "uUnjitterMatrix") \
    X(UNIF_LAST_UNJITTER_MATRIX, "uLastUnjitterMatrix") \
\
    X(UNIF_SHADOW_BIAS_MIN,   "uShadowBiasMin") \
    X(UNIF_SHADOW_BIAS_MAX,   "uShadowBiasMax") \
    X(UNIF_SHADOW_VP_MATRIX,  "uShadowVPMatrix") \
\
    X(UNIF_CONST_DIFFUSE,   "uDiffuse") \
    X(UNIF_CONST_OCCLUSION, "uOcclusion") \
    X(UNIF_CONST_METALLIC,  "uMetallic") \
    X(UNIF_CONST_ROUGHNESS, "uRoughness") \
    X(UNIF_TEX_DIFFUSE,     "texDiffuse") \
    X(UNIF_TEX_OCC_MET_RGH, "texOccMetRgh") \
    X(UNIF_TEX_OCCLUSION,   "texOcclusion") \
    X(UNIF_TEX_METALLIC,    "texMetallic") \
    X(UNIF_TEX_ROUGHNESS,   "texRoughness") \
    X(UNIF_TEX_NORMAL,      "texNormal") \
\
    X(UNIF_TEX_ENVMAP, "texEnvmap") \
    X(UNIF_ENVMAP_DIRECTION, "uEnvmapDirection") \
\
    X(UNIF_TONEMAP_EXPOSURE, "uTonemapExposure") \
    X(UNIF_TONEMAP_ACES_PARAM_A, "uTonemapACESParamA") \
    X(UNIF_TONEMAP_ACES_PARAM_B, "uTonemapACESParamB") \
    X(UNIF_TONEMAP_ACES_PARAM_C, "uTonemapACESParamC") \
    X(UNIF_TONEMAP_ACES_PARAM_D, "uTonemapACESParamD") \
    X(UNIF_TONEMAP_ACES_PARAM_E, "uTonemapACESParamE") \
\
    X(UNIF_TAA_CLAMP_SAMPLE_DIST, "kClampSampleDist") \
    X(UNIF_TAA_FEEDBACK_FACTOR,   "kFeedbackFactor") \
    X(UNIF_SHARPEN_STRENGTH,      "kSharpenStrength") \

// Syntax for textures:
// X(name, target (GL_TEXTURE_*), needs mipmaps, path)

#define XM_ASSETS_TEXTURES \

#define XM_ASSETS_TEXTURES_IGNORED \
    X(TEX_TERRAIN_HEIGHTMAP,        GL_TEXTURE_2D, false, "terrain/testmap.png") \
    X(TEX_TERRAIN_SPLAT0_DIFFUSE,   GL_TEXTURE_2D, true,  "textures/ground0/albedo.jpg") \
    X(TEX_TERRAIN_SPLAT0_NORMAL,    GL_TEXTURE_2D, true,  "textures/ground0/normal.jpg") \
    X(TEX_TERRAIN_SPLAT0_ROUGHNESS, GL_TEXTURE_2D, true,  "textures/ground0/roughness.jpg") \
    X(TEX_TERRAIN_SPLAT1_DIFFUSE,   GL_TEXTURE_2D, true,  "textures/ground1/albedo.jpg") \
    X(TEX_TERRAIN_SPLAT1_NORMAL,    GL_TEXTURE_2D, true,  "textures/ground1/normal.jpg") \
    X(TEX_TERRAIN_SPLAT1_ROUGHNESS, GL_TEXTURE_2D, true,  "textures/ground1/roughness.jpg") \
    X(TEX_TERRAIN_SPLAT2_DIFFUSE,   GL_TEXTURE_2D, true,  "textures/ground2/albedo.jpg") \
    X(TEX_TERRAIN_SPLAT2_NORMAL,    GL_TEXTURE_2D, true,  "textures/ground2/normal.jpg") \
    X(TEX_TERRAIN_SPLAT2_ROUGHNESS, GL_TEXTURE_2D, true,  "textures/ground2/roughness.jpg") \

// Syntax for models:
// X(name, base directory, main GLTF file name)

#define XM_ASSETS_MODELS_GLTF \
    X(MDL_SPONZA,   "models/Sponza",    "Sponza.gltf") \
    X(MDL_DUCK,     "models/Duck",      "Duck.gltf") \
    X(MDL_BOX_MR,   "models/BoxTextured-MetallicRoughness", "BoxTextured.gltf") \
    X(MDL_SPHERES,  "models/MetalRoughSpheres/glTF", "MetalRoughSpheres.gltf") \

#define XM_ASSETS_MODELS_GLTF_IGNORED \

#define XM_ASSETS_FONTS \
    X(FONT_DEFAULT,       "fonts/Roboto-Medium.ttf", 16) \
    X(FONT_DEFAULT_BOLD,  "fonts/Roboto-Black.ttf",  16) \
    X(FONT_DEFAULT_LARGE, "fonts/Roboto-Medium.ttf", 32) \

// Syntax for render targets:
// X(name, format)
// NOTE: render target names should match their corresponding UNIF_RT uniform variable names

#define XM_RENDERTARGETS_SCREEN \
    X(RT_DEPTH,         GL_DEPTH_COMPONENT32F) \
    X(RT_COLOR_LDR,     GL_RGBA8) \
    X(RT_COLOR_HDR,     GL_R11F_G11F_B10F) \
    X(RT_NORMAL,        GL_RGB16F) \
    X(RT_AUX_HDR11,     GL_R11F_G11F_B10F) \
    X(RT_AUX_HDR16,     GL_RGB16F) \
    X(RT_AUX1,          GL_RGBA8) \
    X(RT_AUX2,          GL_RGBA8) \
    X(RT_AUX_DEPTH,     GL_DEPTH_COMPONENT32F) \

#define XM_RENDERTARGETS_SHADOW \
    X(RT_SHADOW_DEPTH, GL_DEPTH_COMPONENT16) \

#define XM_RENDERTARGETS_ENVMAP \
    X(RT_ENVMAP_DEPTH, GL_DEPTH_COMPONENT16) \
    X(RT_ENVMAP_COLOR, GL_RGB16F) \

// Syntax for framebuffers:
// [BEGIN|END](framebuffer name)
// [COLOR|DEPTH](attachment point, render target name)
// Framebuffers can only have one depth attachment.

#define XM_FRAMEBUFFERS \
    BEGIN(FB_ALL) \
        DEPTH(GL_DEPTH_ATTACHMENT, RT_DEPTH) \
        COLOR(GL_COLOR_ATTACHMENT0, RT_COLOR_LDR) \
        COLOR(GL_COLOR_ATTACHMENT1, RT_COLOR_HDR) \
        COLOR(GL_COLOR_ATTACHMENT2, RT_NORMAL) \
        COLOR(GL_COLOR_ATTACHMENT3, RT_AUX_HDR11) \
        COLOR(GL_COLOR_ATTACHMENT4, RT_AUX_HDR16) \
        COLOR(GL_COLOR_ATTACHMENT5, RT_AUX1) \
        COLOR(GL_COLOR_ATTACHMENT6, RT_AUX2) \
    END(FB_ALL) \
\
    BEGIN(FB_GBUFFER) \
        DEPTH(GL_DEPTH_ATTACHMENT, RT_DEPTH) \
        COLOR(GL_COLOR_ATTACHMENT0, RT_COLOR_LDR) \
        COLOR(GL_COLOR_ATTACHMENT1, RT_NORMAL) \
        COLOR(GL_COLOR_ATTACHMENT2, RT_AUX1) \
        COLOR(GL_COLOR_ATTACHMENT3, RT_AUX2) \
        COLOR(GL_COLOR_ATTACHMENT4, RT_AUX_HDR16) \
    END(FB_GBUFFER) \
\
    BEGIN(FB_AUX1_ONLY) \
        COLOR(GL_COLOR_ATTACHMENT0, RT_AUX1) \
    END(FB_AUX1_ONLY) \
\
    BEGIN(FB_AUX2_ONLY) \
        COLOR(GL_COLOR_ATTACHMENT0, RT_AUX2) \
    END(FB_AUX2_ONLY) \
\
    BEGIN(FB_ONLY_COLOR_HDR) \
        DEPTH(GL_DEPTH_ATTACHMENT, RT_DEPTH) \
        COLOR(GL_COLOR_ATTACHMENT0, RT_COLOR_HDR) \
        COLOR(GL_COLOR_ATTACHMENT1, RT_AUX2) \
    END(FB_ONLY_COLOR_HDR) \
\
    BEGIN(FB_ONLY_COLOR_LDR) \
        DEPTH(GL_DEPTH_ATTACHMENT, RT_DEPTH) \
        COLOR(GL_COLOR_ATTACHMENT0, RT_COLOR_LDR) \
        COLOR(GL_COLOR_ATTACHMENT1, RT_AUX2) \
    END(FB_ONLY_COLOR_LDR) \
\
    BEGIN(FB_TAA) \
        DEPTH(GL_DEPTH_ATTACHMENT, RT_DEPTH) \
        COLOR(GL_COLOR_ATTACHMENT0, RT_COLOR_HDR) \
    END(FB_TAA) \
\
    BEGIN(FB_TAA_COPY) \
        COLOR(GL_COLOR_ATTACHMENT0, RT_AUX_HDR11) \
    END(FB_TAA_COPY) \
\
    BEGIN(FB_SHADOW) \
        DEPTH(GL_DEPTH_ATTACHMENT, RT_SHADOW_DEPTH) \
    END(FB_SHADOW) \

// Previous line intentionally left blank.