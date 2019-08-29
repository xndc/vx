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
    #if defined(DEBUG_VIS)
        #if defined(DEBUG_VIS_SHADOWMAP)
            outColor = vec4(texture(gShadow, fragCoord01).r);
        #else
            outColor = texelFetch(gAux2, ivec2(gl_FragCoord.xy), 0);
        #endif
    #else
        vec3 hdr = texelFetch(gColorHDR, ivec2(gl_FragCoord.xy), 0).rgb * uTonemapExposure;

        #if 0
        // Sharpening filter:
        // See "Temporal Antialiasing in Uncharted 4" (Ke Xu, SIGGRAPH 2016)
        vec3 nb1 = texelFetch(gColorHDR, ivec2(gl_FragCoord.xy) + ivec2(+1, +1), 0).rgb * uTonemapExposure;
        vec3 nb2 = texelFetch(gColorHDR, ivec2(gl_FragCoord.xy) + ivec2(+1, -1), 0).rgb * uTonemapExposure;
        vec3 nb3 = texelFetch(gColorHDR, ivec2(gl_FragCoord.xy) + ivec2(-1, +1), 0).rgb * uTonemapExposure;
        vec3 nb4 = texelFetch(gColorHDR, ivec2(gl_FragCoord.xy) + ivec2(-1, -1), 0).rgb * uTonemapExposure;
        hdr = 5 * hdr - nb1 - nb2 - nb3 - nb4;
        #endif

        vec3 ldr;
        #if defined(TONEMAP_LINEAR)
            ldr = hdr;
        #elif defined(TONEMAP_REINHARD)
            ldr = hdr / (1 + hdr);
        #elif defined(TONEMAP_HABLE)
            // John Hable's tonemapping operator, also known as the Uncharted 2 operator:
            // http://filmicworlds.com/blog/filmic-tonemapping-operators/
            const vec3 W = vec3(11.2);
            const vec3 ExposureBias = vec3(2.0);
            ldr = TonemapHable(hdr * ExposureBias) * (1.0 / TonemapHable(W));
        #elif defined(TONEMAP_ACES)
            // Simplified luminance-only ACES filmic tonemapping:
            // https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
            vec3 a = vec3(uTonemapACESParamA);
            vec3 b = vec3(uTonemapACESParamB);
            vec3 c = vec3(uTonemapACESParamC);
            vec3 d = vec3(uTonemapACESParamD);
            vec3 e = vec3(uTonemapACESParamE);
            ldr = (hdr*(a*hdr+b))/(hdr*(c*hdr+d)+e);
        #endif
        outColor = vec4(clamp(ldr, 0.0, 1.0), 1.0);
    #endif
}