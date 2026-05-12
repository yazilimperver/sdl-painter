#version 330 core
in vec2 v_tex_coord;
in vec4 v_color;
out vec4 frag_color;
uniform sampler2D u_texture;
uniform float u_opacity;
void main() {
    vec4 tex_color = texture(u_texture, v_tex_coord);
    frag_color = tex_color * v_color * vec4(1.0, 1.0, 1.0, u_opacity);
}
