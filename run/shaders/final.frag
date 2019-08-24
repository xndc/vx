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

uniform float uTonemapExposure;
#ifdef TONEMAP_ACES
    uniform float uTonemapACESParamA;
    uniform float uTonemapACESParamB;
    uniform float uTonemapACESParamC;
    uniform float uTonemapACESParamD;
    uniform float uTonemapACESParamE;
#endif

vec3 TonemapHable (vec3 x) {
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

void main() {
    vec3 hdr = texelFetch(gColorHDR, ivec2(gl_FragCoord.xy), 0).rgb * uTonemapExposure;
    #ifdef TONEMAP_LINEAR
        vec3 ldr = hdr;
    #elif TONEMAP_REINHARD
        vec3 ldr = hdr / (1 + hdr);
    #elif TONEMAP_HABLE
        // John Hable's tonemapping operator, also known as the Uncharted 2 operator:
        // http://filmicworlds.com/blog/filmic-tonemapping-operators/
        const float W = 11.2;
        const float ExposureBias = 2.0;
        vec3 ldr = TonemapHable(hdr * ExposureBias) * (1.0 / TonemapHable(W));
    #elif TONEMAP_ACES
        // Simplified luminance-only ACES filmic tonemapping:
        // https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
        float a = uTonemapACESParamA;
        float b = uTonemapACESParamB;
        float c = uTonemapACESParamC;
        float d = uTonemapACESParamD;
        float e = uTonemapACESParamE;
        vec3 ldr = (hdr*(a*hdr+b))/(hdr*(c*hdr+d)+e);
    #endif
    outColor = vec4(clamp(ldr, 0.0, 1.0), 1.0);
}