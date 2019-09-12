#version 430

in vec2 f_texcoord;

out vec4 fragColor;

uniform sampler2D tex;

void main() {

	// sample color from texture with uv coordinates(f_texcoord) 
	fragColor = vec4(texture(tex, f_texcoord).rgb, 1.0);
}
