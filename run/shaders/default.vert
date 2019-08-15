#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aTangent;
layout (location = 3) in vec2 aTexcoord0;
layout (location = 4) in vec2 aTexcoord1;
layout (location = 5) in vec3 aColor;
layout (location = 6) in vec3 aJoints;
layout (location = 7) in vec3 aWeights;

out vec3 FragPosClip;
out vec2 TexCoord0;
out vec2 TexCoord1;
out mat3 TBN;
// out float TangentW;

uniform mat4 uModelMatrix;
uniform mat4 uViewMatrix;
uniform mat4 uProjMatrix;

void main() {
    vec4 Clip = uProjMatrix * uViewMatrix * uModelMatrix * vec4(aPosition, 1.0);
    gl_Position = Clip;
    FragPosClip = Clip.xyz;
    TexCoord0 = aTexcoord0;
    TexCoord1 = aTexcoord1;
    #if 0
    mat4 worldToObject = inverse(uModelMatrix);
    mat4 objectToWorld = uModelMatrix;
    vec3 normalWorld = normalize(vec4(aNormal, 1.0) * worldToObject).xyz;
    vec3 tangentWorld = normalize(objectToWorld * aTangent).xyz;
    vec3 binormalWorld = normalize(cross(normalWorld, tangentWorld) * aTangent.w);
    normal = normalWorld;
    #else
    // https://learnopengl.com/Advanced-Lighting/Normal-Mapping
    // FIXME: aTangent.w is a "sign value (-1 or +1) indicating handedness of the tangent basis"
    //   for GLTF models. I'm not sure whether multiplication or division is appropriate, or if I
    //   even have to do something here in the first place.
    // TangentW = aTangent.w;
    vec3 T = normalize((uModelMatrix * vec4(aTangent.xyz, 0.0) / aTangent.w).xyz);
    vec3 N = normalize((uModelMatrix * vec4(aNormal, 0.0)).xyz);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    TBN = mat3(T, B, N);
    #endif
}