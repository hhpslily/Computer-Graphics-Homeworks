#version 430

layout (location = 0) in vec3 v_vertex;
layout (location = 1) in vec3 v_normal;

out vec3 f_vertexInView;
out vec3 f_normalInView;

uniform mat4 um4p;
uniform mat4 um4v;
uniform mat4 um4n;

void main() {

	vec4 vertexInView = um4v * um4n * vec4(v_vertex, 1.0);
	vec4 normalInView = um4v * um4n * vec4(v_normal, 0.0);

	f_vertexInView = vertexInView.xyz;
	f_normalInView = normalInView.xyz;

	gl_Position = um4p * vertexInView;
}
