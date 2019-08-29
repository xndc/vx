#version 330 core
in vec2 fragCoordClip;
in vec2 fragCoord01;

layout(location = 0) out vec4 outColorHDR;
layout(location = 1) out vec4 outAux2;

uniform vec2 iResolution;
uniform float iTime;
uniform int iFrame;

uniform mat4 uViewMatrix;
uniform mat4 uProjMatrix;
uniform mat4 uInvViewMatrix;
uniform mat4 uInvProjMatrix;
uniform mat4 uLastViewMatrix;
uniform mat4 uLastProjMatrix;

uniform mat4 uShadowVPMatrix;
uniform float uShadowBiasMin;
uniform float uShadowBiasMax;

uniform vec3 uAmbientCube[6];
uniform vec3 uSunPosition;
uniform vec3 uSunColor;
uniform vec3 uPointLightPositions[4];
uniform vec3 uPointLightColors[4];

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

#define PI 3.14159265358979323846

const vec3 DielectricSpecular = vec3(0.04);

vec3 SurfaceF0 (vec3 diffuse, float metallic) {
    return mix(DielectricSpecular, diffuse, metallic);
}

vec3 FresnelSchlick (float cosTheta, vec3 F0) {
    // We keep the power over 0 to avoid black spots at random points in the scene.
    return F0 + (1.0 - F0) * max(pow(1.0 - cosTheta, 5.0), 0.001);
}

float DistributionGGX (vec3 N, vec3 H, float roughness) {
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(float NdotV, float NdotL, float Rgh) {
    float ggx1 = GeometrySchlickGGX(NdotL, Rgh);
    float ggx2 = GeometrySchlickGGX(NdotV, Rgh);
    return ggx1 * ggx2;
}

vec3 DirectionalLightLo (vec3 N, vec3 V, vec3 L, vec3 Lcolor, vec3 Diffuse, float Met, float Rgh) {
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float HdotV = max(dot(H, V), 0.0);
    vec3 F = FresnelSchlick(HdotV, SurfaceF0(Diffuse, Met));
    float NDF = DistributionGGX(N, H, Rgh);
    float G = GeometrySmith(NdotV, NdotL, Rgh);
    vec3 Specular = (NDF * G * F) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - Met);
    return (kD * Diffuse / PI + Specular) * Lcolor * NdotL;
}

vec3 PointLightLo (vec3 N, vec3 V, vec3 L, vec3 Lcolor, vec3 Diffuse, float Met, float Rgh) {
    vec3 Lnorm = normalize(L);
    vec3 H = normalize(V + Lnorm);
    float Distance = length(L);
    float Attenuation = 1.0 / (Distance * Distance);
    vec3 Radiance = Lcolor * Attenuation;
    float NdotL = max(dot(N, Lnorm), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float HdotV = max(dot(H, V), 0.0);
    vec3 F = FresnelSchlick(HdotV, SurfaceF0(Diffuse, Met));
    float NDF = DistributionGGX(N, H, Rgh);
    float G = GeometrySmith(NdotV, NdotL, Rgh);
    vec3 Specular = (NDF * G * F) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - Met);
    return (kD * Diffuse / PI + Specular) * Radiance * NdotL;
}

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
    ivec2 fc = ivec2(gl_FragCoord.xy);

    // Don't do lighting calculations for missing fragments (e.g. sky):
    float z = texelFetch(gDepth, fc, 0).r;
    if (z == 0) {
        outColorHDR = texelFetch(gColorLDR, fc, 0);
        return;
    }

    // Correct depth on machines without GL_ARB_clip_control support: (map [1, 0.5] to [1, 0])
    #ifndef DEPTH_ZERO_TO_ONE
        z = z * 2.0 - 1.0;
    #endif

    vec3 diffuse  = texelFetch(gColorLDR, fc, 0).rgb;
    vec3 normal   = texelFetch(gNormal,   fc, 0).rgb;
    vec3 aux1     = texelFetch(gAux1,     fc, 0).rgb;
    vec2 velocity = texelFetch(gAuxHDR16, fc, 0).rg;
    float rough = max(aux1.g, 0.05); // lighting looks wrong around 0
    float metal = aux1.b;

    vec3 CameraPosition = (uInvViewMatrix * vec4(0, 0, 0, 1)).xyz;
    vec4 FragPosClip = vec4(fragCoordClip, z, 1.0);
    // NOTE: Linearized depth is in FragPosView4.w (z is always 1, I think).
    vec4 FragPosView4 = uInvProjMatrix * FragPosClip;
    vec4 FragPosWorld4 = uInvViewMatrix * FragPosView4;
    vec3 FragPosWorld = FragPosWorld4.xyz / FragPosWorld4.w;
    vec3 N = normal; // normals output by the gbuf_main pass should already be normalized
    vec3 V = normalize(CameraPosition - FragPosWorld.xyz);

    vec3 Lo = vec3(0.0);
    #if 1
    // Ambient lighting using HL2-style ambient cube:
    // https://drivers.amd.com/developer/gdc/D3DTutorial10_Half-Life2_Shading.pdf page 59
    vec3 Nsq = N * N;
    ivec3 isNegative = ivec3(N.x < 0.0, N.y < 0.0, N.z < 0.0);
    Lo += diffuse * Nsq.x * uAmbientCube[isNegative.x + 0]    // maps to [0] for X+, [1] for X-
        + diffuse * Nsq.y * uAmbientCube[isNegative.y + 2]    // maps to [2] for Y+, [3] for Y-
        + diffuse * Nsq.z * uAmbientCube[isNegative.z + 4];   // maps to [4] for Z+, [5] for Z-
    #endif

    #if 0
    // Shadow:
    // https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping
    float shadow = 0.0;
    float bias = max(uShadowBiasMax * (1.0 - dot(N, normalize(uSunPosition))), uShadowBiasMin);
    vec4 FragPosShadow = uShadowVPMatrix * FragPosWorld4;
    FragPosShadow /= FragPosShadow.w;
    // outColorHDR = FragPosWorld4; return;
    vec2 ShadowTexcoord = FragPosShadow.xy * 0.5 + 0.5;
    vec2 ShadowTexelSize = (1.0 / textureSize(gShadow, 0)) * 1.0; // tunable, increase for more spread
    if (ShadowTexcoord.x >= 0.0 && ShadowTexcoord.y >= 0.0 && ShadowTexcoord.x <= 1.0 && ShadowTexcoord.y <= 1.0) {
        for (int ipcfX = 0; ipcfX < SHADOW_PCF_TAPS_X; ipcfX++) {
            for (int ipcfY = 0; ipcfY < SHADOW_PCF_TAPS_Y; ipcfY++) {
                // FIXME: This spreads samples fairly well, but is obviously very noisy.
                //   A separate TAA pass for shadows is required. The regular TAA pass doesn't smooth these out,
                //   seemingly due to the way we've implemented neighbourhood clamping,
                vec2 offset = vec2((ipcfX - (SHADOW_PCF_TAPS_X / 2)), (ipcfY - (SHADOW_PCF_TAPS_Y / 2)));
                #if 0 // enable for noisy shadows
                offset.y += rand(V.xy + float((iFrame + 2) % 4)) * 3.0 - 1.5;
                offset.x += rand(V.xy + float((iFrame + 1) % 4)) * 3.0 - 1.5;
                #endif
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
    #endif

    // Filled in by shadow_resolve. 0 = fragment is fully sunlit, 1 = fragment is fully shadowed.
    float shadow = aux1.r;

    // Directional lighting:
    // NOTE: Every vector we pass here needs to be normalized. The Fresnel term gets blown up when
    //   you look at the sun otherwise.
    #if 1
    Lo += mix(vec3(0), DirectionalLightLo(N, V, normalize(uSunPosition), uSunColor, diffuse, metal, rough), 1.0-shadow);
    #endif

    #if 1
    for (int i = 0; i < 4; i++) {
        vec3 L = uPointLightPositions[i] - FragPosWorld;
        Lo += PointLightLo(N, V, L, uPointLightColors[i], diffuse, metal, rough);
    }
    #endif

    outColorHDR = vec4(Lo, 1.0);

    // For debug visualization, we write values into the aux2 buffer and read them out in the final shader.
    // See main.h for details - that's where the enum containing all of the debug vis modes is.
    #if defined(DEBUG_VIS_GBUF_COLOR)
        outAux2 = vec4(diffuse, 1.0);
    #elif defined(DEBUG_VIS_GBUF_NORMAL)
        outAux2 = vec4(normal, 1.0);
    #elif defined(DEBUG_VIS_GBUF_ORM)
        outAux2 = vec4(aux1, 1.0);
    #elif defined(DEBUG_VIS_GBUF_VELOCITY)
        outAux2 = vec4(0.05 + velocity * 4.0, 0.0, 1.0);
    #elif defined(DEBUG_VIS_GBUF_ORM)
        outAux2 = vec4(aux1, 1.0);
    #elif defined(DEBUG_VIS_WORLDPOS)
        outAux2 = vec4(FragPosWorld * 0.01, 1.0);
    #elif defined(DEBUG_VIS_DEPTH_RAW)
        // Encode raw depth into RGBA channels:
        // https://aras-p.info/blog/2009/07/30/encoding-floats-to-rgba-the-final/
        vec4 enc = vec4(1.0, 255.0, 65025.0, 16581375.0) * texelFetch(gDepth, fc, 0).r;
        enc = fract(enc);
        enc -= enc.yzww * vec4(1.0/255.0,1.0/255.0,1.0/255.0,0.0);
        outAux2 = enc;
    #elif defined(DEBUG_VIS_DEPTH_LINEAR)
        outAux2 = vec4(FragPosView4.w);
    #endif
}