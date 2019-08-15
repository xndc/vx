#version 330 core
out vec4 outColor;
in vec2 fragCoord;
in vec2 fragCoord01;
in vec2 fragCoordPx;
uniform ivec2 iResolution;

uniform sampler2D gDepth;
uniform sampler2D gColorLDR;
uniform sampler2D gColorHDR;
uniform sampler2D gNormal;
uniform sampler2D gAuxHDR11;
uniform sampler2D gAuxHDR16;
uniform sampler2D gAux1;
uniform sampler2D gAux2;
uniform sampler2D gAuxDepth;
uniform sampler2D gShadow;

void main() {
    #if 1
    // Simple Reinhard tonemapping:
    // https://learnopengl.com/Advanced-Lighting/HDR
    const float gamma = 2.2;
    // vec3 hdr = texelFetch(gColorHDR, ivec2(gl_FragCoord.xy), 0).rgb;
    vec3 hdr = texelFetch(gAuxHDR16, ivec2(gl_FragCoord.xy), 0).rgb;
    vec3 mapped = hdr / (hdr + vec3(1.0));
    mapped = pow(mapped, vec3(1.0 / gamma));
    outColor = vec4(mapped, 1.0);
    #else
    outColor = vec4(texelFetch(gAuxHDR16, ivec2(gl_FragCoord.xy), 0).rgb, 1.0);
    #endif
}