#version 330 core
in vec4 FragPos;
in vec2 TexCoord0;

uniform int uStipple;
uniform float uStippleHardCutoff;
uniform float uStippleSoftCutoff;
uniform vec4 uDiffuse;

uniform sampler2D texDiffuse;

// https://github.com/hughsk/glsl-dither/blob/master/2x2.glsl

float dither2x2(vec2 position, float brightness) {
  int x = int(mod(position.x, 2.0));
  int y = int(mod(position.y, 2.0));
  int index = x + y * 2;
  float limit = 0.0;

  if (x < 8) {
    if (index == 0) limit = 0.25;
    if (index == 1) limit = 0.75;
    if (index == 2) limit = 1.00;
    if (index == 3) limit = 0.50;
  }

  return brightness < limit ? 0.0 : 1.0;
}

void main() {
    vec4 diffuse = uDiffuse * texture(texDiffuse, TexCoord0);
    if (uStipple != 0) {
        if (diffuse.a < uStippleHardCutoff) {
            discard;
        }
        if (diffuse.a < uStippleSoftCutoff) {
            float a = (diffuse.a - uStippleHardCutoff) / (uStippleSoftCutoff - uStippleHardCutoff);
            if (dither2x2(gl_FragCoord.xy, a) < 0.5) {
                discard;
            }
        }
    }
}