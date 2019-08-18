#version 330 core
in vec2 fragCoordClip;
in vec2 fragCoord01;
layout(location = 0) out vec4 outColor;
uniform vec2 iResolution;
uniform sampler2D texEnvmap;
uniform mat4 uEnvmapDirection;

// https://learnopengl.com/PBR/IBL/Diffuse-irradiance
const vec2 InverseAtan = vec2(0.1591, 0.3183);
vec2 SpheremapUV (vec3 dir) {
    vec2 uv = vec2(atan(dir.z, dir.x), asin(dir.y));
    uv *= InverseAtan;
    uv += 0.5;
    return uv;
}

void main() {
    vec4 wtf = uEnvmapDirection * vec4(fragCoordClip, 1, 1);
    vec3 dir = wtf.xyz / wtf.w;
    vec2 uv = SpheremapUV(normalize(dir));
    outColor = texture(texEnvmap, uv);
}