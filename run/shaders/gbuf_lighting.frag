#version 330 core
in vec2 fragCoordClip;
in vec2 fragCoord01;

layout(location = 0) out vec4 outColorHDR;

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