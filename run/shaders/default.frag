#version 330 core
out vec4 FragColor;

in vec4 gl_FragCoord;
in vec2 vTexcoord;

uniform float fColor;
uniform float fEmissive;
uniform float fMetallic;
uniform float fRoughness;
uniform sampler2D texColor;
uniform sampler2D texExtra;
uniform sampler2D texEmissive;

void main() {
    FragColor = texture(texColor, vTexcoord);
}