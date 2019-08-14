#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aTangent;
layout (location = 3) in vec2 aTexcoord0;
layout (location = 4) in vec2 aTexcoord1;
layout (location = 5) in vec3 aColor;
layout (location = 6) in vec3 aJoints;
layout (location = 7) in vec3 aWeights;

out vec2 texcoord0;
out vec2 texcoord1;
out vec3 normal;

uniform mat4 uModelMatrix;
uniform mat4 uViewMatrix;
uniform mat4 uProjMatrix;

void main() {
    gl_Position = uProjMatrix * uViewMatrix * uModelMatrix * vec4(aPosition, 1.0);
    texcoord0 = aTexcoord0;
    texcoord1 = aTexcoord1;
    // vec3 normalWorld = mat3(transpose(inverse(uModelMatrix))) * aNormal;
    mat4 worldToObject = inverse(uModelMatrix);
    mat4 objectToWorld = uModelMatrix;
    vec3 normalWorld = normalize(vec4(aNormal, 1.0) * worldToObject).xyz;
    vec3 tangentWorld = normalize(objectToWorld * aTangent).xyz;
    vec3 binormalWorld = normalize(cross(normalWorld, tangentWorld) * aTangent.w);
    normal = normalWorld;
}