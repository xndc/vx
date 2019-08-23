#version 330 core
layout (location = 0) in vec3 aPosition;
out vec2 fragCoordClip;     // Position of fragment coordinate within viewport, -1 to 1.
out vec2 fragCoord01;       // Position of fragment coordinate within viewport, 0 to 1.

// Vertex shader for full-screen passes.
// Maps a standard upwards-facing quad from [X,Y] [-1,-1] to [1,1] to the clip space, at Z=1.0 (before everything else).

void main() {
    gl_Position = vec4(aPosition.xz, 1.0, 1.0);
    fragCoordClip = aPosition.xz;
    fragCoord01 = aPosition.xz * 0.5 + 0.5;
}