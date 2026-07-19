#version 330 core

// Final pass: the crisp scene plus the blurred bloom added on top. Rendered to
// the widget's default framebuffer. The additive bloom is what turns the plain
// wireframe into a glowing neon shape.

in vec2 uv;
uniform sampler2D scene;
uniform sampler2D bloom;
uniform float intensity;
uniform float exposure;   // pre-tonemap gain; keeps the line cores bright

// Floor glow: a soft gradient rising from the bottom-centre, so the shape reads
// as sitting on a faintly lit ground plane (matches the reference image).
uniform vec3  floorColor;
uniform float floorIntensity;

out vec4 frag;

// ACES filmic tone-map (Narkowicz 2015 fit). Rolls highlights off smoothly
// toward 1.0 instead of hard-clamping, so however much additive bloom stacks up
// it self-compresses below the ceiling rather than blowing out to a flat white
// blob. This is the "auto-adjust": brighter glow -> more compression.
vec3 aces(vec3 x)
{
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 s = texture(scene, uv).rgb;
    vec3 bl = texture(bloom, uv).rgb;

    // A true ground strip BELOW the shape: full strength only at the bottom
    // edge (uv.y = 0), completely gone by ~30% up the viewport. The previous
    // smoothstep(0, k, 1-uv.y) form SATURATED for everything below (1-k) of the
    // screen height, which is why the glow used to flood most of the frame.
    float vert = 1.0 - smoothstep(0.0, 0.30, uv.y);
    vert *= vert;   // quadratic falloff -> decays quickly as it rises
    float horiz = clamp(1.0 - abs(uv.x - 0.5) * 0.9, 0.0, 1.0); // gentle centre bias
    vec3 floorGlow = floorColor * (vert * horiz) * floorIntensity;

    // Tone-map ONLY the glow (bloom + floor). Where there's no glow the result is
    // 0, so the background stays pure black and the crisp line cores in `s` stay
    // white; only the stacked halos self-compress toward 1.0 instead of clipping
    // to a flat ceiling. This is the auto-adjust: the brighter the glow, the
    // harder it rolls off.
    vec3 glow = aces((bl * intensity + floorGlow) * exposure);
    frag = vec4(min(s + glow, vec3(1.0)), 1.0);
}
