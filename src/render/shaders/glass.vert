#version 330 core

in vec3 position;
in vec3 normal;

uniform mat4 mvp;
uniform mat4 modelView;
uniform mat3 normalMatrix;

out vec3 fragNormal;
out vec3 fragViewDir;

void main() {
    vec4 viewPos = modelView * vec4(position, 1.0);
    fragNormal  = normalize(normalMatrix * normal);
    fragViewDir = normalize(-viewPos.xyz); // direction to camera
    gl_Position = mvp * vec4(position, 1.0);
}