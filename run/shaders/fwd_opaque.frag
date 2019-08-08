#version 330 core
in vec2 texcoord0;
in vec2 texcoord1;

layout(location = 0) out vec4 outColorLDR;
layout(location = 1) out vec4 outColorHDR;
layout(location = 2) out vec4 outColorTAA;
layout(location = 3) out vec4 outNormal;
layout(location = 4) out vec4 outVelocity;
layout(location = 5) out vec4 outAux1;
layout(location = 6) out vec4 outAux2;

uniform float uDiffuse;
uniform float uMetallic;
uniform float uRoughness;
uniform sampler2D texDiffuse;
uniform sampler2D texOccMetRgh;
uniform sampler2D texMetallic;
uniform sampler2D texRoughness;

uniform sampler2D gDepth;
uniform sampler2D gColorLDR;
uniform sampler2D gColorHDR;
uniform sampler2D gColorTAA;
uniform sampler2D gNormal;
uniform sampler2D gVelocity;
uniform sampler2D gAux1;
uniform sampler2D gAux2;
uniform sampler2D gShadow;

void main() {
    vec4 diffuse = texture(texDiffuse, texcoord0);
    if (diffuse.a < 0.1) {
        discard;
    }
    float metallic  = uMetallic  + texture(texOccMetRgh, texcoord0).g + texture(texMetallic,  texcoord0).r;
    float roughness = uRoughness + texture(texOccMetRgh, texcoord0).b + texture(texRoughness, texcoord0).r;

    outColorLDR = diffuse;
    outAux1 = vec4(0, metallic, roughness, 0);
}