#version 330 core
layout (location = 0) in vec2 aPosition;
out vec2 fragCoord;         // Position of fragment coordinate within viewport, -1 to 1.
out vec2 fragCoord01;       // Position of fragment coordinate within viewport, 0 to 1.
out vec2 fragCoordPx;       // Position of fragment coordinate within viewport, in pixels.
uniform ivec2 iResolution;  // Viewport resolution in pixels.

// Vertex shader for full-screen passes.
// Maps 2D vertices with positions in the [-1,1] range to the Z=0 plane.

void main() {
    gl_Position = vec4(aPosition, 0.0, 1.0);
    fragCoord = aPosition;
    fragCoord01 = aPosition * 0.5 + 0.5;
    fragCoordPx = fragCoord01 * vec2(iResolution);
}