#version 330 core
layout (location = 0) in vec2 aPosition;
out vec2 fragCoordClip;     // Position of fragment coordinate within viewport, -1 to 1.
out vec2 fragCoord01;       // Position of fragment coordinate within viewport, 0 to 1.

// Vertex shader for full-screen passes.
// Maps 2D vertices with positions in the [-1,1] range to the Z=0 plane.

void main() {
    gl_Position = vec4(aPosition, 0.0, 1.0);
    fragCoordClip = aPosition;
    fragCoord01 = aPosition * 0.5 + 0.5;
}