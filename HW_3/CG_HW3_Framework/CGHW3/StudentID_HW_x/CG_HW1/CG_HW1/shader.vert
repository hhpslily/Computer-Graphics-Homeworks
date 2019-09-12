#version 430

layout (location = 0) in vec3 v_vertex;
layout (location = 1) in vec2 v_texcoord;

uniform mat4 um4mvp;

out vec2 f_texcoord;

void main() {

	f_texcoord = v_texcoord;

	gl_Position = um4mvp * vec4(v_vertex, 1.0);
}
