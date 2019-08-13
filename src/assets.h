#pragma once

#define XM_ASSETS_SHADERS \
    X(VSH_DEFAULT,          "shaders/default.vert") \
    X(VSH_FULLSCREEN_PASS,  "shaders/fullscreen.vert") \
    X(FSH_FWD_OPAQUE,       "shaders/fwd_opaque.frag") \
    X(FSH_FX_DITHER,        "shaders/fx_dither.frag") \
    X(FSH_FINAL,            "shaders/final.frag") \

#define XM_ASSETS_PROGRAMS \
    X(PROG_FWD_OPAQUE, VSH_DEFAULT, FSH_FWD_OPAQUE) \
    X(PROG_FX_DITHER,  VSH_FULLSCREEN_PASS, FSH_FX_DITHER) \
    X(PROG_FINAL,      VSH_FULLSCREEN_PASS, FSH_FINAL) \

// Syntax for attributes:
// X(location global name, layout location index, GLSL name, GLTF name)
// See https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#meshes for
// GLTF attribute names.

#define XM_ATTRIBUTE_LOCATIONS \
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

#define XM_ASSETS_SHADER_UNIFORMS \
    X(UNIF_RT_DEPTH,            "gDepth") \
    X(UNIF_RT_COLOR_LDR,        "gColorLDR") \
    X(UNIF_RT_COLOR_HDR,        "gColorHDR") \
    X(UNIF_RT_COLOR_TAA,        "gColorTAA") \
    X(UNIF_RT_NORMAL,           "gNormal") \
    X(UNIF_RT_VELOCITY,         "gVelocity") \
    X(UNIF_RT_AUX1,             "gAux1") \
    X(UNIF_RT_AUX2,             "gAux2") \
    X(UNIF_RT_SHADOW_DEPTH,     "gShadow") \
    X(UNIF_IRESOLUTION,         "iResolution") \
    X(UNIF_STIPPLE,             "uStipple") \
    X(UNIF_STIPPLE_HARD_CUTOFF, "uStippleHardCutoff") \
    X(UNIF_STIPPLE_SOFT_CUTOFF, "uStippleSoftCutoff") \
    X(UNIF_MODEL_MATRIX,        "uModelMatrix") \
    X(UNIF_VIEW_MATRIX,         "uViewMatrix") \
    X(UNIF_PROJ_MATRIX,         "uProjMatrix") \
    X(UNIF_CONST_DIFFUSE,       "uDiffuse") \
    X(UNIF_CONST_METALLIC,      "uMetallic") \
    X(UNIF_CONST_ROUGHNESS,     "uRoughness") \
    X(UNIF_TEX_DIFFUSE,         "texDiffuse") \
    X(UNIF_TEX_OCC_MET_RGH,     "texOccMetRgh") \
    X(UNIF_TEX_OCCLUSION,       "texOcclusion") \
    X(UNIF_TEX_METALLIC,        "texMetallic") \
    X(UNIF_TEX_ROUGHNESS,       "texRoughness") \
    X(UNIF_TEX_NORMAL,          "texNormal") \

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

#define XM_ASSETS_MODELS_GLTF_IGNORED \

#define XM_ASSETS_FONTS \
    X(FONT_ROBOTO_MEDIUM_16, "fonts/Roboto-Medium.ttf", 16) \
    X(FONT_ROBOTO_BOLD_16,   "fonts/Roboto-Black.ttf",  16) \
    X(FONT_ROBOTO_MEDIUM_32, "fonts/Roboto-Medium.ttf", 32) \

// Syntax for render targets:
// X(name, format)
// NOTE: render target names should match their corresponding UNIF_RT uniform variable names

#define XM_ASSETS_RENDERTARGETS_SCREENSIZE \
    X(RT_COLOR_LDR,     GL_RGBA8) \
    X(RT_COLOR_HDR,     GL_RGB16F) \
    X(RT_COLOR_TAA,     GL_RGB16F) \
    X(RT_NORMAL,        GL_RGB16F) \
    X(RT_VELOCITY,      GL_RG16F) \
    X(RT_AUX1,          GL_RGBA8) \
    X(RT_AUX2,          GL_RGBA8) \
    X(RT_DEPTH,         GL_DEPTH32F_STENCIL8) \

#define XM_ASSETS_RENDERTARGETS_SHADOWSIZE \
    X(RT_SHADOW_DEPTH, GL_DEPTH_COMPONENT32F) \

// Syntax for framebuffers:
// [BEGIN|END](framebuffer name)
// [COLOR|DEPTH](attachment point, render target name)
// Framebuffers can only have one depth attachment.

#define XM_ASSETS_FRAMEBUFFERS \
    BEGIN(FB_MAIN) \
        DEPTH(GL_DEPTH_STENCIL_ATTACHMENT, RT_DEPTH) \
        COLOR(GL_COLOR_ATTACHMENT0, RT_COLOR_LDR) \
        COLOR(GL_COLOR_ATTACHMENT1, RT_COLOR_HDR) \
        COLOR(GL_COLOR_ATTACHMENT2, RT_COLOR_TAA) \
        COLOR(GL_COLOR_ATTACHMENT3, RT_NORMAL) \
        COLOR(GL_COLOR_ATTACHMENT4, RT_VELOCITY) \
        COLOR(GL_COLOR_ATTACHMENT5, RT_AUX1) \
        COLOR(GL_COLOR_ATTACHMENT6, RT_AUX2) \
    END(FB_MAIN) \
    BEGIN(FB_SHADOW) \
        DEPTH(GL_DEPTH_ATTACHMENT, RT_SHADOW_DEPTH) \
    END(FB_SHADOW) \

// Previous line intentionally left blank.