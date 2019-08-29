#version 330 core
out vec4 outAux1;
in vec2 fragCoordClip;
in vec2 fragCoord01;

uniform vec2 iResolution;
uniform float iTime;
uniform int iFrame;

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
uniform mat4 uShadowVPMatrix;
uniform float uShadowBiasMin;
uniform float uShadowBiasMax;

// Resolves shadows into gAux1.r. This is ostensibly our occlusion buffer, but we don't support occlusion anyway.
// Uses gAux2 as a TAA accumulation buffer for shadows. gAux1 should be copied to gAux2 before running this pass.
// References:
// * https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping
// * https://developer.nvidia.com/gpugems/GPUGems2/gpugems2_chapter17.html (Example 17-2)

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

    float z = texture(gDepth, fragCoord01).r;
    vec3 CameraPosition = (uInvViewMatrix * vec4(0, 0, 0, 1)).xyz;
    vec4 FragPosClip = vec4(fragCoordClip, z, 1.0);
    vec4 FragPosView4 = uInvProjMatrix * FragPosClip;
    vec4 FragPosWorld4 = uInvViewMatrix * FragPosView4;
    vec3 FragPosWorld = FragPosWorld4.xyz / FragPosWorld4.w;
    vec3 N = texture(gNormal, fragCoord01).rgb;
    vec3 V = normalize(CameraPosition - FragPosWorld.xyz);

    float bias = max(uShadowBiasMax * (1.0 - dot(N, normalize(uSunPosition))), uShadowBiasMin);
    vec4 FragPosShadow = uShadowVPMatrix * FragPosWorld4;
    FragPosShadow /= FragPosShadow.w;
    vec2 ShadowTexcoord = FragPosShadow.xy * 0.5 + 0.5;
    vec2 ShadowTexelSize = (1.0 / textureSize(gShadow, 0)) * 1.0; // tunable, increase for more spread

    if (ShadowTexcoord.x >= 0.0 && ShadowTexcoord.y >= 0.0 && ShadowTexcoord.x <= 1.0 && ShadowTexcoord.y <= 1.0) {
        for (int ipcfX = 0; ipcfX < SHADOW_PCF_TAPS_X; ipcfX++) {
            for (int ipcfY = 0; ipcfY < SHADOW_PCF_TAPS_Y; ipcfY++) {
                vec2 offset = vec2((ipcfX - (SHADOW_PCF_TAPS_X / 2)), (ipcfY - (SHADOW_PCF_TAPS_Y / 2)));
                offset.y += rand(V.xy + float((iFrame + 2) % 4)) * 3.0 - 1.5;
                offset.x += rand(V.xy + float((iFrame + 1) % 4)) * 3.0 - 1.5;
                float zShadowMap = texture(gShadow, ShadowTexcoord + offset * ShadowTexelSize).r;
                // Same depth correction we do for the main depth buffer in NEGATIVE_ONE_TO_ONE mode:
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

    vec4 aux1 = texelFetch(gAux1, ivec2(gl_FragCoord.xy), 0);

    vec2 vel = texture(gAuxHDR16, fragCoord01).rg;
    vec2 uvhist = fragCoord01 - vel;
    vec4 aux2 = texture(gAux2, uvhist);
    float accum = aux2.r;

    if (uvhist.x < 0 || uvhist.y < 0 || uvhist.x > 1 || uvhist.y > 1 || isnan(accum) ||
        abs(accum - shadow) > 0.95 || abs(aux2.g - aux1.g) > 0.01 || abs(aux2.b - aux1.b) > 0.01)
    {
        accum = shadow;
    }

    outAux1 = vec4(mix(shadow, accum, 0.9), aux1.gba);
}