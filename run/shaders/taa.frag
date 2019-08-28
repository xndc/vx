#version 330 core
out vec4 outColorHDR;
in vec2 fragCoordClip;

uniform ivec2 iResolution;

uniform sampler2D gDepth;
uniform sampler2D gColorLDR;
uniform sampler2D gColorHDR;
uniform sampler2D gNormal;
uniform sampler2D gAuxHDR11;
uniform sampler2D gAuxHDR16;
uniform sampler2D gAux1;
uniform sampler2D gAux2;
uniform sampler2D gAuxDepth;
uniform sampler2D gShadow;

uniform mat4 uInvViewMatrix;
uniform mat4 uInvProjMatrix;
uniform mat4 uLastViewMatrix;
uniform mat4 uLastProjMatrix;

// Add 0.5*uJitter to UV coordinates to unjitter (well, somewhat).
uniform vec2 uJitter;
uniform vec2 uJitterLast;

// This is largely based on "Temporal Reprojection Anti-Aliasing in INSIDE".
// References:
// * "Temporal Reprojection Anti-Aliasing in INSIDE" (GDC 2016)
//   http://twvideo01.ubm-us.net/o1/vault/gdc2016/Presentations/Pedersen_LasseJonFuglsang_TemporalReprojectionAntiAliasing.pdf
// * "High Quality Temporal Supersampling" (SIGGRAPH 2014) (Unreal Engine)
//   https://de45xmedrsdbp.cloudfront.net/Resources/files/TemporalAA_small-59732822.pdf
// * "Graphics Gems from CryENGINE 3" (SIGGRAPH 2013)

void main() {
    // Retrieve (unjittered) input fragment colour:
    vec2 uv = fragCoordClip * 0.5 + 0.5 + 0.5*uJitter;
    vec3 inputColor = texture(gColorHDR, uv).rgb;

    // Find closest-to-camera fragment in 3x3 region around input fragment:
    // This apparently results in nicer results around edges. Some materials call this "velocity dilation".
    float closestDepth = 0.0;
    ivec2 closestPixel = ivec2(gl_FragCoord.xy);
    for (int y = -2; y <= 2; y++) {
        for (int x = -2; x <= 2; x++) {
            ivec2 pixel = ivec2(gl_FragCoord.xy) + ivec2(x, y);
            float depth = texelFetch(gDepth, pixel, 0).r;
            if (depth > closestDepth) {
                closestDepth = depth;
                closestPixel = pixel;
            }
        }
    }

    // Retrieve UV-space velocity of selected fragment (generated in gbuf_main):
    vec2 vel = texelFetch(gAuxHDR16, closestPixel, 0).rg;
    vec2 uvhist = uv - vel;

    // Reproject fragment into history buffer (gAuxHDR11) and retrieve accumulated value for it:
    vec3 accumColor = texture(gAuxHDR11, uvhist).rgb;
    // Compensate for bad accumulation buffer values:
    if (closestDepth == 0 || uvhist.x < 0 || uvhist.y < 0 || uvhist.x > 1 || uvhist.y > 1 ||
        isnan(accumColor) != bvec3(false)) {
        accumColor = inputColor;
    }

    // Neighbourhood clamping:
    // We sample some fragments around the current one to figure out what the correct historical value might reasonably
    // be, assuming the history buffer contains correct fragments and not garbage, and then we clamp the historical
    // value to this range.
    // References:
    // * "Temporal Reprojection Anti-Aliasing in INSIDE" (GDC 2016) -- the explanation I used
    // *  -- referenced as the original source
    const float kClampSampleDist = 0.5;
    vec3 nb1 = texture(gColorHDR, uv + vec2(+kClampSampleDist, +kClampSampleDist) / vec2(iResolution)).rgb;
    vec3 nb2 = texture(gColorHDR, uv + vec2(+kClampSampleDist, -kClampSampleDist) / vec2(iResolution)).rgb;
    vec3 nb3 = texture(gColorHDR, uv + vec2(-kClampSampleDist, +kClampSampleDist) / vec2(iResolution)).rgb;
    vec3 nb4 = texture(gColorHDR, uv + vec2(-kClampSampleDist, -kClampSampleDist) / vec2(iResolution)).rgb;
    vec3 nbMin = min(min(min(nb1, nb2), nb3), nb4); // per-component min
    vec3 nbMax = max(max(max(nb1, nb2), nb3), nb4); // per-component max
    vec3 accumColorClamped = clamp(accumColor, nbMin, nbMax);

    // Final blend:
    const float kFeedbackFactor = 0.75; // higher values => more sample retention, smoother image
    outColorHDR = vec4(mix(inputColor, accumColorClamped, vec3(kFeedbackFactor)), 0);
}