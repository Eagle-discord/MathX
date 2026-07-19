#version 330 core

// Bright-pass prefilter: keeps only the portion of each pixel brighter than
// `threshold`, so the bloom halo comes from the glowing shape and not from a
// (possibly non-black) background. On the default black background this is
// nearly a straight copy of the lit shape.

in vec2 uv;
uniform sampler2D scene;
uniform float threshold;
out vec4 frag;

void main()
{
    vec3 c = texture(scene, uv).rgb;
    float b = max(c.r, max(c.g, c.b));          // luminance proxy
    float k = max(b - threshold, 0.0) / max(b, 1e-4);
    frag = vec4(c * k, 1.0);
}
