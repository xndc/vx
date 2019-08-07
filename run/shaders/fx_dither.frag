#version 330 core
layout(location = 0) out vec4 outColorLDR;
layout(location = 1) out vec4 outColorHDR;
layout(location = 2) out vec4 outColorTAA;
layout(location = 3) out vec4 outNormal;
layout(location = 4) out vec4 outVelocity;
layout(location = 5) out vec4 outAux1;
layout(location = 6) out vec4 outAux2;
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

const float OrderedDitheringMatrix[16] = float[] (
    0.0f,  8.0f,  2.0f,  10.0f,
    12.0f, 4.0f,  14.0f, 6.0f,
    3.0f,  11.0f, 1.0f,  9.0f,
    15.0f, 7.0f,  13.0f, 5.0f
);

void main() {
    int xm = int(mod(fragCoordPx.x/2.0, 2.0));
    int ym = int(mod(fragCoordPx.y/2.0, 2.0));
    float dither = OrderedDitheringMatrix[xm + ym * 4];
    vec4 origColor = texture2D(gColorLDR, fragCoord01);

    float r = origColor.r;
    r += 0.315f;
    r -= dither / 35.0f;
    r = floor(r * 10) / 10;

    float g = origColor.g;
    g += 0.32f;
    g -= dither / 35.0f;
    g = floor(g * 10) / 10;

    float b = origColor.b;
    b += 0.305f;
    b -= dither / 35.0f;
    b = floor(b * 10) / 10;

    outColorLDR = vec4(r, g, b, 1.0f);
    outAux1 = texture2D(gAux1, fragCoord01);
}