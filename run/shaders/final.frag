#version 330 core
out vec4 outColor;
in vec2 fragCoord;
in vec2 fragCoord01;
in vec2 fragCoordPx;
uniform ivec2 iResolution;

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
    outColor = texture2D(gColorLDR, fragCoord01);
}