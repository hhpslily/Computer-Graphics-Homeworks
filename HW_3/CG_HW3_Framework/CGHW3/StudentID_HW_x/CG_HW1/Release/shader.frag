#version 430

in vec3 f_vertexInView;
in vec3 f_normalInView;

out vec4 fragColor;

struct LightInfo{
	vec4 position;		// Light position in eye coords
	vec4 spotDirection;
	vec4 La;			// Ambient light intensity
	vec4 Ld;			// Diffuse light intensity
	vec4 Ls;			// Specular light intensity
	float spotExponent;
	float spotCutoff;
	float constantAttenuation;
	float linearAttenuation;
	float quadraticAttenuation;
};

struct MaterialInfo
{
	vec4 Ka;
	vec4 Kd;
	vec4 Ks;
	float shininess;
};

uniform int lightIdx;
uniform mat4 um4v;
uniform LightInfo light[3];
uniform MaterialInfo material;

vec4 directionalLight(vec3 N, vec3 V){
	vec4 lightInView = um4v * light[0].position;
	vec3 S = normalize(lightInView.xyz);
	vec3 H = normalize(S + V);

	float dc = max(dot(S, N), 0);
	float sc = pow(max(dot(H, N), 0), 64);

	return light[0].La * material.Ka + dc * light[0].Ld * material.Kd + sc * light[0].Ls * material.Ks;
}

vec4 pointLight(vec3 N, vec3 V){
	vec4 lightInView = um4v * light[1].position;
	vec3 S = normalize(lightInView.xyz - f_vertexInView);
	vec3 H = normalize(S + V);
	float dc = max(dot(S, N), 0);
	float sc = pow(max(dot(H, N), 0), 64);

	float l = length(lightInView.xyz - f_vertexInView);
	float att = light[1].constantAttenuation + light[1].linearAttenuation * l + light[1].quadraticAttenuation * (l*l);

	return light[1].La * material.Ka + dc * light[1].Ld * material.Kd / att + sc * light[1].Ls * material.Ks;
}

vec4 spotLight(vec3 N, vec3 V){
	vec4 lightInView = um4v * light[2].position;
	vec3 S = normalize(lightInView.xyz - f_vertexInView);
	vec3 d = normalize(light[2].spotDirection.xyz);

	float angle = acos(dot(-S, d));
	float cutoff = radians(clamp(light[2].spotCutoff, 0, 90));
	if(angle < cutoff){
		float spotFactor = pow(dot(-S, d), light[2].spotExponent);
		vec3 H = normalize(S + V);

		float dc = max(dot(S, N), 0);
		float sc = pow(max(dot(H, N), 0), 64);

		return light[2].La * material.Ka + spotFactor * (dc * light[2].Ld * material.Kd + sc * light[2].Ls * material.Ks);
	}
	else{
		return light[2].La * material.Ka;
	}
}

void main() {
	vec3 N = normalize(f_normalInView);
	vec3 V = normalize(-f_vertexInView);
	
	vec4 color = vec4(0, 0, 0, 0);

	if(lightIdx == 0)
	{
		color += directionalLight(N, V);
	}
	else if(lightIdx == 1)
	{
		color += pointLight(N, V);
	}
	else if(lightIdx == 2)
	{
		color += spotLight(N ,V);
	}

	fragColor = color;
}
