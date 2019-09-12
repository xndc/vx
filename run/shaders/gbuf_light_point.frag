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
uniform mat4 uVP;
uniform mat4 uVPInv;
uniform mat4 uVPLast;
uniform vec3 uCameraPos;

uniform vec3 uPointLightPosition;
uniform vec3 uPointLightColor;

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

// Evaluates the Unreal Engine 4 variant of the Cook-Torrance BRDF:
// * Lambertian diffuse term.
// * Specular D: Trowbridge-Reitz (GGX) normal distribution function.
// * Specular F: Schlick's approximation of the Fresnel term.
// * Specular G: Schlick's approximation of Bruce G. Smith's geometric occlusion function.
// References:
// * Real Shading in Unreal Engine 4:
//   - https://blog.selfshadow.com/publications/s2013-shading-course/
//   - https://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
// * https://learnopengl.com/PBR/Theory

#define PI 3.14159265358979323846

vec3 LambertDiffuse (vec3 diffuse, vec3 specularF, float metallic) {
    #if 1
        // Version used by LearnOpenGL. Darkens diffuse reflections from metallic objects.
        // Looks more physically correct, at least for our use case (no IBL support yet).
        return (vec3(1) - specularF) * (1.0 - metallic) * diffuse / PI;
    #else
        // Version used by Unreal, or at least the one described by Brian Karis.
        // Probably works better when you actually have IBL support.
        return diffuse/PI;
    #endif
}

vec3 FresnelSchlick (float HdotV, vec3 diffuse, float metallic) {
    // F0 estimation technique taken from https://learnopengl.com/PBR/Theory
    vec3 F0 = mix(vec3(0.04), diffuse, metallic);
    return F0 + (1.0 - F0) * pow(2.0, (-5.55473*HdotV - 6.98316) * HdotV);
}

float DistributionGGX (float NdotH, float roughness) {
    float alpha = roughness * roughness; // Disney remapping
    float alpha2 = alpha * alpha;
    float NdotH2 = NdotH*NdotH;
    float x = (NdotH2 * (alpha2 - 1.0) + 1.0);
    return alpha2 / (PI * x * x);
}

float GeometrySchlickSmith (float NdotV, float NdotL, float Rgh) {
    float k = (Rgh+1)*(Rgh+1) / 8.0; // Disney + Unreal remapping
    float g1 = NdotL / ((NdotL * (1 - k)) + k);
    float g2 = NdotV / ((NdotV * (1 - k)) + k);
    return g1 * g2;
}

vec3 PointLightLo (vec3 N, vec3 V, vec3 L, vec3 Lcolor, vec3 Diffuse, float Met, float Rgh) {
    float Distance = length(L);
    L = normalize(L);
    vec3 H = normalize(V + L);
    float Attenuation = 1.0 / (Distance * Distance);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float HdotV = max(dot(H, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float SpecD = DistributionGGX(NdotH, Rgh);
    vec3  SpecF = FresnelSchlick(HdotV, Diffuse, Met);
    float SpecG = GeometrySchlickSmith(NdotV, NdotL, Rgh);
    vec3 Diff = LambertDiffuse(Diffuse, SpecF, Met);
    vec3 BRDF = Diff + (SpecD * SpecF * SpecG) / max(4 * NdotL * NdotV, 0.001);
    return BRDF * Lcolor * Attenuation * NdotL;
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
    // FIXME: Replace this with a discard once we have an actual skybox shader.
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
    vec3 aux1     = texelFetch(gAux1,     fc, 0).rgb;
    vec2 velocity = texelFetch(gAuxHDR16, fc, 0).rg;
    float rough = max(aux1.g, 0.1); // lighting looks wrong around 0 (specular highlight goes away entirely)
    float metal = aux1.b;

    // Retrieve world-space position of fragment:
    vec4 FragPosClip = vec4(fragCoordClip, z, 1.0);
    vec4 FragPosWorld4 = uVPInv * FragPosClip;
    vec3 FragPosWorld = FragPosWorld4.xyz / FragPosWorld4.w;

    vec3 N = texelFetch(gNormal, fc, 0).rgb; // normals output by gbuf_main should already be normalized
    vec3 V = normalize(uCameraPos - FragPosWorld.xyz);

    vec3 L = uPointLightPosition - FragPosWorld;
    vec3 Lo = PointLightLo(N, V, L, uPointLightColor, diffuse, metal, rough);
    outColorHDR = texelFetch(gColorHDR, fc, 0) + vec4(Lo, 1.0);

    outAux2 = texelFetch(gAux2, fc, 0); // keep info in aux2 (debug vis)
}