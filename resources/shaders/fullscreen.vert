#version 330 core

// Fullscreen quad drawn as a triangle strip in NDC. Passes a 0..1 UV to the
// fragment stage for the post-process passes (prefilter / blur / composite).

in vec2 position;
out vec2 uv;

void main()
{
    uv = position * 0.5 + 0.5;
    gl_Position = vec4(position, 0.0, 1.0);
}
