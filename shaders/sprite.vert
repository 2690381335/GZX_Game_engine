#version 450
layout(location = 0) in vec4 rectangle;
layout(location = 1) in vec4 color;
layout(location = 2) in vec4 uv_bounds;
layout(location = 3) in vec4 transform;
layout(push_constant) uniform FrameData {
    vec2 extent;
    vec2 camera_position;
    float zoom;
} frame_data;
layout(location = 0) out vec4 vertex_color;
layout(location = 1) out vec2 vertex_uv;

const vec2 corners[6] = vec2[](
    vec2(0, 0), vec2(1, 0), vec2(1, 1),
    vec2(0, 0), vec2(1, 1), vec2(0, 1));

void main() {
    vec2 corner = corners[gl_VertexIndex];
    vec2 pivot = transform.xy * rectangle.zw;
    vec2 local = corner * rectangle.zw - pivot;
    vec2 rotated = vec2(
        local.x * transform.w - local.y * transform.z,
        local.x * transform.z + local.y * transform.w);
    vec2 pixel = rectangle.xy + pivot + rotated;
    pixel = (pixel - frame_data.camera_position) * frame_data.zoom;
    gl_Position = vec4(pixel / frame_data.extent * 2.0 - 1.0, 0, 1);
    vertex_color = color;
    vertex_uv = mix(uv_bounds.xy, uv_bounds.zw, corner);
}