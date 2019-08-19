#version 330 core
in vec3 CubemapPosition;
layout(location = 0) out vec4 outColorHDR;
uniform samplerCube texEnvmap;

void main() {
    // https://learnopengl.com/PBR/IBL/Diffuse-irradiance
    vec3 envColor = texture(texEnvmap, CubemapPosition).rgb;
    // envColor = envColor / (envColor + vec3(1.0));
    // envColor = pow(envColor, vec3(1.0/2.2));
    outColorHDR = vec4(envColor, 1.0);
}