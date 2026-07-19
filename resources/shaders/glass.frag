#version 330 core

in vec3 fragNormal;
in vec3 fragViewDir;
in vec3 fragPos;

uniform vec3 shapeColor;
uniform float opacity;

out vec4 fragColor;

void main()
{
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(fragViewDir);

    //--------------------------------------
    // Light
    //--------------------------------------

    vec3 L = normalize(vec3(-0.4, 1.3, 0.6));

    //--------------------------------------
    // Ambient
    //--------------------------------------

    float ambientStrength = 0.22;
    vec3 ambient = ambientStrength * shapeColor;

    //--------------------------------------
    // Diffuse (Lambert)
    //--------------------------------------

    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * shapeColor;

    //--------------------------------------
    // Blinn-Phong Specular
    //--------------------------------------

    vec3 H = normalize(L + V);

    float spec = pow(max(dot(N, H), 0.0), 32.0);

    vec3 specular = vec3(0.85) * spec * 0.45;

    //--------------------------------------
    // Rim Lighting
    //--------------------------------------

    float rim = pow(1.0 - max(dot(N, V), 0.0), 4.0);

    vec3 rimLight = vec3(0.16) * rim;

    //--------------------------------------
    // Final Color
    //--------------------------------------

    vec3 color =
        ambient +
        diffuse * 0.78 +
        specular +
        rimLight;

    //--------------------------------------
    // Gamma Correction
    //--------------------------------------

    color = pow(color, vec3(1.0 / 2.2));

    //--------------------------------------
    // Transparency
    //--------------------------------------

    float alpha = mix(opacity, 1.0, rim * 0.35);

    fragColor = vec4(color, alpha);
}