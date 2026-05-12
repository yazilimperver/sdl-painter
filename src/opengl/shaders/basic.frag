#version 330 core
in vec4 v_color;
out vec4 frag_color;
uniform float u_opacity;
void main() {
    frag_color = v_color * vec4(1.0, 1.0, 1.0, u_opacity);
}
