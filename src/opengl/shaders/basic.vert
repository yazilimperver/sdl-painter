#version 330 core
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec4 a_color;
uniform mat3 u_model;
uniform mat4 u_projection;
out vec4 v_color;
void main() {
    vec3 transformed = u_model * vec3(a_position, 1.0);
    gl_Position = u_projection * vec4(transformed.xy, 0.0, 1.0);
    v_color = a_color;
}
