#version 330 core

in vec3 fragNormal;
in vec3 fragViewDir;

uniform vec3 shapeColor;   // your RGB sliders
uniform float opacity;     // base transparency

out vec4 fragColor;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(fragViewDir);

    // Fresnel - edges more opaque than center
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 2.5);

    // Specular highlight - single light from top-left
    vec3 L = normalize(vec3(-0.5, 1.0, 0.8));
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 64.0);

    // Combine
    vec3 baseColor  = shapeColor * 0.4;               // dark tinted base
    vec3 fresnelCol = shapeColor + vec3(0.3);          // brighter at edges
    vec3 color = mix(baseColor, fresnelCol, fresnel) + vec3(spec * 0.6);

    float alpha = opacity + fresnel * 0.5;             // edges more opaque
    fragColor = vec4(color, clamp(alpha, 0.0, 1.0));
}