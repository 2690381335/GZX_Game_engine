#version 450
layout(set = 0, binding = 0) uniform sampler2D sprite_texture;
layout(location = 0) in vec4 vertex_color;
layout(location = 1) in vec2 vertex_uv;
layout(location = 0) out vec4 fragment_color;

void main() {
    fragment_color = texture(sprite_texture, vertex_uv) * vertex_color;
}