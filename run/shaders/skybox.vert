#version 330 core
layout (location = 0) in vec3 aPosition;
uniform mat4 uViewMatrix;
uniform mat4 uProjMatrix;
out vec3 CubemapPosition;

void main() {
    // https://learnopengl.com/PBR/IBL/Diffuse-irradiance
    CubemapPosition = aPosition;
    mat4 rotView = mat4(mat3(uViewMatrix)); // remove translation from the view matrix
    vec4 clipPos = uProjMatrix * rotView * vec4(CubemapPosition, 1.0);
    gl_Position = vec4(clipPos.xy, 0.0, clipPos.w);
}