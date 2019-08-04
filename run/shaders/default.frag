#version 330 core
out vec4 FragColor;

in vec4 gl_FragCoord;
in vec2 vTexcoord;

uniform float uDiffuse;
uniform float uMetallic;
uniform float uRoughness;
uniform sampler2D texDiffuse;
uniform sampler2D texOccMetRgh;
uniform sampler2D texMetallic;
uniform sampler2D texRoughness;
uniform sampler2D gDepth;
uniform sampler2D gColorLDR;
uniform sampler2D gColorHDR;
uniform sampler2D gColorTAA;
uniform sampler2D gNormal;
uniform sampler2D gVelocity;
uniform sampler2D gAux1;
uniform sampler2D gAux2;
uniform sampler2D gShadow;

void main() {
    vec4 diffuse = texture(texDiffuse, vTexcoord);
    if (diffuse.a < 0.1) {
        discard;
    }
    FragColor = diffuse;
}