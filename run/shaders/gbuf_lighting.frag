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

uniform vec3 uAmbientZP;
uniform vec3 uAmbientZN;
uniform vec3 uAmbientYP;
uniform vec3 uAmbientYN;
uniform vec3 uAmbientXP;
uniform vec3 uAmbientXN;
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

    // Ambient lighting:
    vec3 lightf = vec3(0.3);

    // Directional lighting:
    vec3 lightDir = normalize(vec3(100, 100, 100) - world);
    vec3 diffuse = max(dot(norm, lightDir), 0.0) * color * vec3(3.0, 3.0, 3.0);
    lightf += diffuse;

    #if 0
    vec3 lightDir = normalize((-uSunDirection) - world);
    vec3 factorSunlight = max(dot(norm, lightDir), 0.0) * uSunColor;

    // HACK: We don't support full ambient lighting, so just average our colours out:
    vec3 factorAmbient = (uAmbientZP + uAmbientZN + uAmbientYP + uAmbientYN + uAmbientXP + uAmbientXN) / 6.0;

    vec3 final = (factorSunlight + factorAmbient) * color;
    #endif

    // There are certainly better ways to do this.
    if (texelFetch(gDepth, fc, 0).r == 0.0) {
        outColorHDR = vec4(0.8f, 1.1f, 1.8f, 1.0f);
    } else {
        outColorHDR = vec4(lightf * color, 1.0);
    }
}