#version 330 core
in vec2 fragCoordClip;
in vec2 fragCoord01;

layout(location = 0) out vec4 outColorHDR;
layout(location = 1) out vec4 outAux2;

uniform vec2 iResolution;
uniform float iTime;
uniform uint iFrame;

uniform mat4 uViewMatrix;
uniform mat4 uProjMatrix;
uniform mat4 uInvViewMatrix;
uniform mat4 uInvProjMatrix;

uniform vec3 uAmbientCube[6];
// uniform vec3 uAmbientZP;
// uniform vec3 uAmbientZN;
// uniform vec3 uAmbientYP;
// uniform vec3 uAmbientYN;
// uniform vec3 uAmbientXP;
// uniform vec3 uAmbientXN;
uniform vec3 uSunDirection;
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

vec3 SurfaceF0 (vec3 diffuse, float metallic) {
    return mix(vec3(0.04), diffuse, metallic);
}

vec3 FresnelSchlick (float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
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

float GeometrySchlickGGX(float NdotV, float roughness)
{
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

vec3 DirectionalLight (vec3 N, vec3 V, vec3 L, vec3 Lcolor, vec3 Diffuse, float Met, float Rgh) {
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
    // return vec3(F);
    return (kD * Diffuse / PI + Specular) * Lcolor * NdotL;
}

void main() {
    ivec2 fc = ivec2(gl_FragCoord.xy);
    vec3 diffuse = texelFetch(gColorLDR, fc, 0).rgb;
    vec3 normal  = texelFetch(gNormal,   fc, 0).rgb;
    vec3 aux1    = texelFetch(gAux1,     fc, 0).rgb;
    float metal = aux1.g;
    float rough = aux1.b;

    // NOTE: I have no idea why we need to do this. I guess OpenGL screws our Z up somehow?
    float z = texelFetch(gDepth, fc, 0).r * 2.0 - 1.0;
    vec3 CameraPosition = (uInvViewMatrix * vec4(0, 0, 0, 1)).xyz;
    vec4 FragPosClip = vec4(fragCoordClip, z, 1.0);
    vec4 FragPosView = uInvProjMatrix * FragPosClip;
    FragPosView /= FragPosView.w;
    vec3 FragPosWorld = (uInvViewMatrix * FragPosView).xyz;
    vec3 N = normal; // normals output by the gbuf_main pass should already be normalized
    vec3 V = normalize(CameraPosition - FragPosWorld);

    vec3 Lo = vec3(0.0);
    // Ambient lighting using HL2-style ambient cube:
    // https://drivers.amd.com/developer/gdc/D3DTutorial10_Half-Life2_Shading.pdf page 59
    vec3 Nsq = N * N;
    ivec3 isNegative = ivec3(N.x < 0.0, N.y < 0.0, N.z < 0.0);
    Lo += diffuse * Nsq.y * uAmbientCube[isNegative.y]        // maps to [0] for Y+, [1] for Y-
        + diffuse * Nsq.z * uAmbientCube[isNegative.z + 2]    // maps to [2] for Z+, [3] for Z-
        + diffuse * Nsq.x * uAmbientCube[isNegative.x + 4];   // maps to [4] for X+, [5] for X-

    // Directional lighting:
    // NOTE: uSunDirection is supposed to be the vector coming FROM the light, but for some reason
    //   our DirectionalLight function is treating it as the sun's POSITION vector. I have no clue
    //   what we're doing wrong.
    // NOTE: Also, every vector we pass here needs to be normalized. The Fresnel term gets blown
    //   up when you look at the sun otherwise.
    #if 1
    Lo += DirectionalLight(N, V, normalize(-uSunDirection), uSunColor, diffuse, metal, rough);
    #else
    // Treat the point lights we're passing as directional ones, for testing.
    for (int i = 0; i < 4; i++) {
        Lo += DirectionalLight(N, V, normalize(uPointLightPositions[i]), 0.08 * uPointLightColors[i],
            diffuse, metal, rough);
    }
    #endif

    #if 0
    for (int i = 0; i < 4; i++) {
        vec3 L = normalize(uPointLightPositions[i] - worldPos);
        vec3 H = normalize(V + L);
        float distance = length(uPointLightPositions[i] - worldPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = uPointLightColors[i] * attenuation;
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), SurfaceF0(diffuse, metal));
        float NDF = DistributionGGX(N, H, rough);
        float G = GeometrySmith(N, V, L, rough);
        vec3 num = NDF * G * F;
        float denom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
        vec3 specular = num / max(denom, 0.001);
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metal;
        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * diffuse / PI + specular) * radiance * NdotL;
    }
    #endif

    // There are certainly better ways to do this.
    if (texelFetch(gDepth, fc, 0).r == 0.0) {
        outColorHDR = vec4(0.8f, 1.1f, 1.8f, 1.0f);
    } else {
        outColorHDR = vec4(Lo, 1.0);
    }

    // outColorHDR = texelFetch(gAux2, fc, 0);
    // outColorHDR = vec4(N, 1);
}

/*
void main() {
    ivec2 fc = ivec2(gl_FragCoord.xy);
    vec3 color  = texelFetch(gColorLDR, fc, 0).rgb;
    vec3 normal = texelFetch(gNormal,   fc, 0).rgb;
    vec3 aux1   = texelFetch(gAux1,     fc, 0).rgb;
    float met = aux1.g;
    float rgh = aux1.b;

    // Get world position from depth:
    float z = texelFetch(gDepth, fc, 0).r * 2.0 - 1.0;
    vec4 clip = vec4(fragCoordClip, z, 1.0);
    vec4 view = uInvProjMatrix * clip;
    view /= view.w;
    vec3 world = (uInvViewMatrix * view).xyz;
    vec3 norm = normalize(normal);
    vec3 viewDir = normalize(view.xyz - world);

    vec3 lightf = vec3(0);

    // Ambient lighting using HL2-style ambient cube:
    // https://drivers.amd.com/developer/gdc/D3DTutorial10_Half-Life2_Shading.pdf page 59
    vec3 nsq = norm * norm;
    ivec3 isNegative = ivec3(norm.x < 0.0, norm.y < 0.0, norm.z < 0.0);
    lightf += nsq.y * uAmbientCube[isNegative.y]        // maps to [0] for Y+, [1] for Y-
            + nsq.z * uAmbientCube[isNegative.z + 2]    // maps to [2] for Z+, [3] for Z-
            + nsq.x * uAmbientCube[isNegative.x + 4];   // maps to [4] for X+, [5] for X-

    // Directional lighting:
    vec3 lightDir = -uSunDirection;
    vec3 diffuse = max(dot(norm, lightDir), 0.0) * uSunColor;
    lightf += diffuse;

    // There are certainly better ways to do this.
    if (texelFetch(gDepth, fc, 0).r == 0.0) {
        outColorHDR = vec4(0.8f, 1.1f, 1.8f, 1.0f);
    } else {
        outColorHDR = vec4(lightf * color, 1.0);
    }
}
*/