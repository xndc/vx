#pragma once

#define XM_ASSETS_SHADERS \
    X(VSH_DEFAULT,  "shaders/default.vert") \
    X(FSH_DEFAULT,  "shaders/default.frag") \

#define XM_ASSETS_SHADER_ATTRIBUTES \
    X(ATTR_POSITION,    0, "aPosition") \
    X(ATTR_NORMAL,      1, "aNormal") \
    X(ATTR_TANGENT,     2, "aTangent") \
    X(ATTR_TEXCOORD0,   3, "aTexcoord0") \
    X(ATTR_TEXCOORD1,   4, "aTexcoord1") \
    X(ATTR_COLOR,       5, "aColor") \
    X(ATTR_JOINTS,      6, "aJoints") \
    X(ATTR_WEIGHTS,     7, "aWeights") \

#define XM_ASSETS_SHADER_UNIFORMS \
    X(UNIF_MODEL_MATRIX,    "uModelMatrix") \
    X(UNIF_VIEW_MATRIX,     "uViewMatrix") \
    X(UNIF_PROJ_MATRIX,     "uProjMatrix") \
    X(UNIF_CONST_DIFFUSE,   "uDiffuse") \
    X(UNIF_CONST_METALLIC,  "uMetallic") \
    X(UNIF_CONST_ROUGHNESS, "uRoughness") \
    X(UNIF_TEX_DIFFUSE,     "texDiffuse") \
    X(UNIF_TEX_OCC_MET_RGH, "texOccMetRgh") \
    X(UNIF_TEX_OCCLUSION,   "texOcclusion") \
    X(UNIF_TEX_METALLIC,    "texMetallic") \
    X(UNIF_TEX_ROUGHNESS,   "texRoughness") \
    X(UNIF_TEX_NORMAL,      "texNormal") \
    X(UNIF_RT_DEPTH,        "gDepth") \
    X(UNIF_RT_COLOR_LDR,    "gColorLDR") \
    X(UNIF_RT_COLOR_HDR,    "gColorHDR") \
    X(UNIF_RT_COLOR_TAA,    "gColorTAA") \
    X(UNIF_RT_NORMAL,       "gNormal") \
    X(UNIF_RT_VELOCITY,     "gVelocity") \
    X(UNIF_RT_AUX1,         "gAux1") \
    X(UNIF_RT_AUX2,         "gAux2") \
    X(UNIF_RT_SHADOW_DEPTH, "gShadow") \

#define XM_ASSETS_TEXTURES \

#define XM_ASSETS_TEXTURES_IGNORED \
    X(TEX_TERRAIN_HEIGHTMAP,        "terrain/testmap.png") \
    X(TEX_TERRAIN_SPLAT0_DIFFUSE,   "textures/ground0/albedo.jpg") \
    X(TEX_TERRAIN_SPLAT0_NORMAL,    "textures/ground0/normal.jpg") \
    X(TEX_TERRAIN_SPLAT0_ROUGHNESS, "textures/ground0/roughness.jpg") \
    X(TEX_TERRAIN_SPLAT1_DIFFUSE,   "textures/ground1/albedo.jpg") \
    X(TEX_TERRAIN_SPLAT1_NORMAL,    "textures/ground1/normal.jpg") \
    X(TEX_TERRAIN_SPLAT1_ROUGHNESS, "textures/ground1/roughness.jpg") \
    X(TEX_TERRAIN_SPLAT2_DIFFUSE,   "textures/ground2/albedo.jpg") \
    X(TEX_TERRAIN_SPLAT2_NORMAL,    "textures/ground2/normal.jpg") \
    X(TEX_TERRAIN_SPLAT2_ROUGHNESS, "textures/ground2/roughness.jpg") \

#define XM_ASSETS_MODELS_GLTF \
    X(MDL_SPONZA,   "models/Sponza",    "Sponza.gltf") \
    X(MDL_DUCK,     "models/Duck",      "Duck.gltf") \
    X(MDL_BOX_MR,   "models/BoxTextured-MetallicRoughness", "BoxTextured.gltf") \

#define XM_ASSETS_MODELS_GLTF_IGNORED \

#define XM_ASSETS_FONTS \
    X(FONT_ROBOTO_MEDIUM_16, "fonts/Roboto-Medium.ttf", 16) \
    X(FONT_ROBOTO_MEDIUM_32, "fonts/Roboto-Medium.ttf", 32) \

#define XM_ASSETS_RENDERTARGETS_SCREENSIZE \
    X(RT_DEPTH,         GL_DEPTH32F_STENCIL8) \
    X(RT_COLOR_LDR,     GL_RGBA8) \
    X(RT_COLOR_HDR,     GL_RGB16F) \
    X(RT_COLOR_TAA,     GL_RGB16F) \
    X(RT_NORMAL,        GL_RGB16F) \
    X(RT_VELOCITY,      GL_RG16F) \
    X(RT_AUX1,          GL_RGBA8) \
    X(RT_AUX2,          GL_RGBA8) \

#define XM_ASSETS_RENDERTARGETS_SHADOWSIZE \
    X(RT_SHADOW_DEPTH, GL_DEPTH_COMPONENT32F) \

#define XM_ASSETS_FRAMEBUFFERS \
    BEGIN(FB_MAIN) \
        ATTACH(GL_DEPTH_STENCIL_ATTACHMENT, RT_DEPTH) \
        ATTACH(GL_COLOR_ATTACHMENT0, RT_COLOR_LDR) \
        ATTACH(GL_COLOR_ATTACHMENT1, RT_COLOR_HDR) \
        ATTACH(GL_COLOR_ATTACHMENT2, RT_COLOR_TAA) \
        ATTACH(GL_COLOR_ATTACHMENT3, RT_NORMAL) \
        ATTACH(GL_COLOR_ATTACHMENT4, RT_VELOCITY) \
        ATTACH(GL_COLOR_ATTACHMENT5, RT_AUX1) \
        ATTACH(GL_COLOR_ATTACHMENT6, RT_AUX2) \
    END(FB_MAIN) \
    BEGIN(FB_SHADOW) \
        ATTACH(GL_DEPTH_ATTACHMENT, RT_SHADOW_DEPTH) \
    END(FB_SHADOW) \

// Previous line intentionally left blank.