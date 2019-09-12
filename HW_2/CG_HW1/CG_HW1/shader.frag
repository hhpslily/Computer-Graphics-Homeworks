#version 430

in vec3 f_vertexInView;
in vec3 f_normalInView;

out vec4 fragColor;

struct LightInfo{
	vec4 position;
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

uniform int lightIdx;			// Use this variable to contrl lighting mode
uniform mat4 um4v;				// Camera viewing transformation matrix
uniform LightInfo light[3];
uniform MaterialInfo material;

vec4 directionalLight(vec3 N, vec3 V){

	vec4 lightInView = um4v * light[0].position;	// the position of the light in camera space
	vec3 S = normalize(lightInView.xyz);			// Normalized lightInView
	vec3 H = normalize(S + V);						// Half vector

	// [TODO] calculate diffuse coefficient and specular coefficient here
	float dc = max(dot(N, S), 0);	
	float sc = pow(max(dot(N, H), 0), 64);

	return light[0].La * material.Ka + dc * light[0].Ld * material.Kd + sc * light[0].Ls * material.Ks;
}

vec4 pointLight(vec3 N, vec3 V){

	// [TODO] Calculate point light intensity here
	vec4 lightInView = um4v * light[1].position;
	vec3 S = normalize(lightInView.xyz - f_vertexInView);	
	vec3 H = normalize(S + V);						
	float c1 = light[1].constantAttenuation;
	float c2 = light[1].linearAttenuation;
	float c3 = light[1].quadraticAttenuation;
	float d = length(f_vertexInView - lightInView.xyz); 
	float attenuation = min(1/(c1 + c2*d + c3*d*d), 1);
	float dc = max(dot(N, S), 0);	
	float sc = pow(max(dot(N, H), 0), 64);

	return light[1].La * material.Ka + attenuation * (dc * light[1].Ld * material.Kd + sc * light[1].Ls * material.Ks);
}

vec4 spotLight(vec3 N, vec3 V){
	
	//[TODO] Calculate spot light intensity here
	vec4 lightInView = um4v * light[2].position;
	vec3 S = normalize(lightInView.xyz - f_vertexInView);	
	vec3 H = normalize(S + V);						
	float c1 = light[2].constantAttenuation;
	float c2 = light[2].linearAttenuation;
	float c3 = light[2].quadraticAttenuation;
	float d = length(f_vertexInView - lightInView.xyz); 
	float attenuation = min(1/(c1 + c2*d + c3*d*d), 1);
	float dc = max(dot(N, S), 0);	
	float sc = pow(max(dot(N, H), 0), 64);

	float angle = acos(dot(-S, normalize((um4v * light[2].spotDirection).xyz)));
	float cutoff = radians(clamp(light[2].spotCutoff, 0.0, 90.0));
	float spotFactor;
	if(angle < cutoff){
		spotFactor = pow(dot(-S, normalize((um4v * light[2].spotDirection).xyz)), light[2].spotExponent) * 5;
	}
	else{
		spotFactor = 0;
	}

	return light[2].La * material.Ka + spotFactor * attenuation * (dc * light[2].Ld * material.Kd + sc * light[2].Ls * material.Ks);
}

void main() {

	vec3 N = normalize(f_normalInView);		// N represents normalized normal of the model in camera space
	vec3 V = normalize(-f_vertexInView);	// V represents the vector from the vertex of the model to the camera position
	
	vec4 color = vec4(0, 0, 0, 0);

	// Handle lighting mode
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
