#version 330 core

// One axis of a separable Gaussian blur. `dir` is the per-tap step in UV space:
// (texelWidth, 0) for the horizontal pass, (0, texelHeight) for the vertical.
// Run twice (H then V) per blur iteration; a few iterations widen the halo.

in vec2 uv;
uniform sampler2D src;
uniform vec2 dir;
out vec4 frag;

void main()
{
    // 9-tap Gaussian weights (normalized).
    const float w0 = 0.227027;
    const float w1 = 0.1945946;
    const float w2 = 0.1216216;
    const float w3 = 0.054054;
    const float w4 = 0.016216;

    vec3 r = texture(src, uv).rgb * w0;
    r += texture(src, uv + dir * 1.0).rgb * w1;
    r += texture(src, uv - dir * 1.0).rgb * w1;
    r += texture(src, uv + dir * 2.0).rgb * w2;
    r += texture(src, uv - dir * 2.0).rgb * w2;
    r += texture(src, uv + dir * 3.0).rgb * w3;
    r += texture(src, uv - dir * 3.0).rgb * w3;
    r += texture(src, uv + dir * 4.0).rgb * w4;
    r += texture(src, uv - dir * 4.0).rgb * w4;
    frag = vec4(r, 1.0);
}
