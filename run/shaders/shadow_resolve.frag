#version 330 core
out vec4 outAux1;
in vec2 fragCoordClip;
in vec2 fragCoord01;

uniform ivec2 iResolution;
uniform float iTime;
uniform int iFrame;
uniform vec2 uJitter;
uniform vec2 uJitterLast;

uniform sampler2D gColorLDR;
uniform sampler2D gColorHDR;
uniform sampler2D gNormal;
uniform sampler2D gAux1;
uniform sampler2D gAux2;
uniform sampler2D gAuxHDR16;
uniform sampler2D gDepth;
uniform sampler2D gShadow;

uniform vec3 uSunPosition;
uniform mat4 uInvViewMatrix;
uniform mat4 uInvProjMatrix;
uniform mat4 uLastViewMatrix;
uniform mat4 uLastProjMatrix;
uniform mat4 uVP;
uniform mat4 uVPInv;
uniform mat4 uVPLast;
uniform vec3 uCameraPos;
uniform vec3 uCameraPosLast;

uniform mat4 uShadowVPMatrix;
uniform float uShadowBiasMin;
uniform float uShadowBiasMax;

// Resolves shadows into gAux1.r. This is ostensibly our occlusion buffer, but we don't support occlusion anyway.
// Uses gAux2 as a TAA accumulation buffer for shadows. gAux1 should be copied to gAux2 before running this pass.

// Shadow samples are offset using random noise to get a better distribution. The noise doesn't play well with our
// standard TAA pass (I assume because we do neighbourhood clamping on RGB and not just chroma), so we have an extra TAA
// pass just for the shadows that doesn't use clamping. We instead discard samples based on a few (hacky) heuristics.
// This is awful, a complete waste of GPU time and doesn't even work well, but does result in *slightly* nicer shadows.
// If you use it together with high PCF and don't look at objects too closely, that is. Otherwise it ends up being
// super-noisy in motion.

// More or less "standard" random function.
// http://byteblacksmith.com/improvements-to-the-canonical-one-liner-glsl-rand-for-opengl-es-2-0/
highp float rand (vec2 co) {
    highp float a = 12.9898;
    highp float b = 78.233;
    highp float c = 43758.5453;
    highp float dt = dot(co.xy, vec2(a,b));
    highp float sn = mod(dt,3.14);
    return fract(sin(sn) * c);
}

void main() {
    float shadow = 0.0;

    // Retrieve and, in NEGATIVE_ONE_TO_ONE mode, correct depth:
    float Z = texture(gDepth, fragCoord01).r;
    #ifndef DEPTH_ZERO_TO_ONE
        Z = Z * 2.0 - 1.0;
    #endif

    // Retrieve world-space position for fragment:
    vec4 FragPosClip = vec4(fragCoordClip, Z, 1.0);
    vec4 FragPosWorld4 = uVPInv * FragPosClip;
    vec3 FragPosWorld = FragPosWorld4.xyz / FragPosWorld4.w;

    // Used for bias and discard:
    vec3 N = texture(gNormal, fragCoord01 - 0.5*uJitter).rgb;

    // Used as source of random noise:
    vec3 V = normalize(uCameraPos - FragPosWorld.xyz);

    // Retrieve shadow-space position for fragment:
    vec4 FragPosShadow = uShadowVPMatrix * FragPosWorld4;
    FragPosShadow /= FragPosShadow.w;
    vec2 ShadowTexcoord = FragPosShadow.xy * 0.5 + 0.5;
    vec2 ShadowTexelSize = (1.0 / textureSize(gShadow, 0)) * 1.0; // tunable, increase for larger penumbras

    // Compute shadow bias:
    float bias = max(uShadowBiasMax * (1.0 - dot(N, normalize(uSunPosition))), uShadowBiasMin);

    // Shadow resolve:
    if (ShadowTexcoord.x >= 0.0 && ShadowTexcoord.y >= 0.0 && ShadowTexcoord.x <= 1.0 && ShadowTexcoord.y <= 1.0) {
        for (int ipcfX = 0; ipcfX < SHADOW_PCF_TAPS_X; ipcfX++) {
            for (int ipcfY = 0; ipcfY < SHADOW_PCF_TAPS_Y; ipcfY++) {
                // PCF + random noise offset:
                vec2 offset = vec2((ipcfX - (SHADOW_PCF_TAPS_X / 2)), (ipcfY - (SHADOW_PCF_TAPS_Y / 2)));
                offset *= 1.0; // tunable, increase for larger penumbras
                #ifdef SHADOW_NOISE
                    offset.x += rand(100*FragPosWorld.xy + float((iFrame + 1) % 100)) * 2.0 - 1.0;
                    offset.y += rand(100*FragPosWorld.yz + float((iFrame + 2) % 100)) * 2.0 - 1.0;
                #endif
                float zShadowMap = texture(gShadow, ShadowTexcoord + offset * ShadowTexelSize).r;
                // Correct shadow depth if required:
                #ifndef DEPTH_ZERO_TO_ONE
                    zShadowMap = zShadowMap * 2.0 - 1.0;
                #endif
                if (zShadowMap > FragPosShadow.z + bias) {
                    shadow += 1.0;
                }
            }
        }
    }
    shadow /= SHADOW_PCF_TAPS_X * SHADOW_PCF_TAPS_Y;

    // Retrieve current fragment data from RT_AUX1 (we need to pass it through):
    vec4 aux1 = texelFetch(gAux1, ivec2(gl_FragCoord.xy), 0);

    #ifdef SHADOW_TAA
        // Reproject fragment into shadow history buffer (RT_AUX2.r):
        vec2 vel = texture(gAuxHDR16, fragCoord01).rg;
        vec2 uvhist = fragCoord01 - vel;
        float accum = texture(gAux2, uvhist).r;

        // Same thing for normal:
        vec3 Nhist = texture(gNormal, uvhist).rgb;
        float Ndist = distance(N, Nhist);

        // Discard sample based on a few heuristics, in an attempt to keep ghosting under control:
        // If the camera is stationary, we only want to reject samples that fall outside of the history buffer.
        if (uvhist.x < 0 || uvhist.y < 0 || uvhist.x > 1 || uvhist.y > 1 || isnan(accum)) {
            accum = shadow;
        }
        // If the camera is moving, we want to be fairly aggressive when rejecting samples. Otherwise, we can skip the
        // discard step entirely, to prevent very obvious noise caused by camera rotation. (Noise caused by camera
        // motion is non-trivial to remove, but also isn't quite as obvious.)
        // FIXME: Perform abs(distance(...)) on the CPU side and send it as a uniform.
        if (abs(distance(uCameraPos, uCameraPosLast)) > 0.01 && (
            abs(accum - shadow) > 0.9 || abs(vel.x+vel.y) > 0.001 || Ndist > 0.001)) {
            accum = shadow;
        }
        outAux1 = vec4(mix(shadow, accum, 0.9), aux1.gba);
    #else
        outAux1 = vec4(shadow, aux1.gba);
    #endif
}