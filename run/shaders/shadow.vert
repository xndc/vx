#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 3) in vec2 aTexcoord0;

out vec4 FragPos;
out vec2 TexCoord0;

uniform mat4 uModelMatrix;
uniform mat4 uViewMatrix;
uniform mat4 uProjMatrix;

void main() {
    vec4 PclipThis = uProjMatrix * uViewMatrix * uModelMatrix * vec4(aPosition, 1.0);
    gl_Position = PclipThis;
    TexCoord0 = aTexcoord0;
}