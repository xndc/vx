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

vec3 LambertDiffuse (vec3 diffuse, float metallic) {
    // Lambertian diffuse used by Unreal (Cdiff/PI) with remapping as per the glTF standard.
    // Indistinguishable from the LearnOpenGL diffuse factor (which depends on specular F).
    return mix(diffuse * 0.96, vec3(0), vec3(metallic))/PI;
}

vec3 FresnelSchlick (float HdotV, vec3 diffuse, float metallic) {
    // F0 computation as per the glTF standard:
    vec3 F0 = mix(vec3(0.04), diffuse, metallic);
    // Unreal's modified version of the Schlick Frenel approximation:
    return F0 + (1.0 - F0) * pow(2.0, (-5.55473*HdotV - 6.98316) * HdotV);
}

float DistributionGGX (float NdotH, float roughness) {
    // Roughness remapping as per the glTF standard (originally from Disney's PBR model):
    float alpha = roughness * roughness;
    // Trowbridge-Reitz (GGX) normal distribution function:
    float alpha2 = alpha * alpha;
    float NdotH2 = NdotH*NdotH;
    float x = (NdotH2 * (alpha2 - 1.0) + 1.0);
    return alpha2 / (PI * x * x);
}

float GeometrySchlickSmith (float NdotV, float NdotL, float Rgh) {
    // Disney remapping (Rgh = (Rgh+1)/2) plus Unreal's remapping (k = Rgh/2):
    float k = (Rgh+1)*(Rgh+1) / 8.0;
    // Schlick's approximation of Bruce Smith's geometric attenuation term:
    float g1 = NdotL / ((NdotL * (1 - k)) + k);
    float g2 = NdotV / ((NdotV * (1 - k)) + k);
    return g1 * g2;
}

// Computes the contribution of a directional light to a particular fragment's colour.
vec3 DirectionalLightLo (vec3 N, vec3 V, vec3 L, vec3 Lcolor, vec3 Diffuse, float Met, float Rgh) {
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float HdotV = max(dot(H, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float SpecD = DistributionGGX(NdotH, Rgh);
    vec3  SpecF = FresnelSchlick(HdotV, Diffuse, Met);
    float SpecG = GeometrySchlickSmith(NdotV, NdotL, Rgh);
    vec3 Diff = LambertDiffuse(Diffuse, Met);
    vec3 BRDF = Diff + (SpecD * SpecF * SpecG) / max(4 * NdotL * NdotV, 0.001);
    return BRDF * Lcolor * NdotL;
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

    vec3 Lo = vec3(0.0);
    #if 1
    // Ambient lighting using HL2-style ambient cube:
    // https://drivers.amd.com/developer/gdc/D3DTutorial10_Half-Life2_Shading.pdf page 59
    vec3 Nsq = max(N * N, 0.001); // not clamping causes black spots in some areas
    ivec3 isNegative = ivec3(N.x < 0.0, N.y < 0.0, N.z < 0.0);
    Lo += diffuse * Nsq.x * uAmbientCube[isNegative.x + 0]    // maps to [0] for X+, [1] for X-
        + diffuse * Nsq.y * uAmbientCube[isNegative.y + 2]    // maps to [2] for Y+, [3] for Y-
        + diffuse * Nsq.z * uAmbientCube[isNegative.z + 4];   // maps to [4] for Z+, [5] for Z-
    #endif

    // Filled in by shadow_resolve. 0 = fragment is fully sunlit, 1 = fragment is fully shadowed.
    float shadow = aux1.r;

    // Directional lighting:
    // NOTE: Every vector we pass here needs to be normalized. The Fresnel term gets blown up when
    //   you look at the sun otherwise.
    #if 1
    Lo += mix(vec3(0), DirectionalLightLo(N, V, normalize(uSunPosition), uSunColor, diffuse, metal, rough), 1.0-shadow);
    #endif

    outColorHDR = vec4(Lo, 1.0);

    // For debug visualization, we write values into the aux2 buffer and read them out in the final shader.
    // See main.h for details - that's where the enum containing all of the debug vis modes is.
    #if defined(DEBUG_VIS_GBUF_COLOR)
        outAux2 = vec4(diffuse, 1.0);
    #elif defined(DEBUG_VIS_GBUF_NORMAL)
        outAux2 = vec4(N, 1.0);
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
        vec4 FragPosView4 = uInvProjMatrix * FragPosClip;
        outAux2 = vec4(FragPosView4.w);
    #endif
}