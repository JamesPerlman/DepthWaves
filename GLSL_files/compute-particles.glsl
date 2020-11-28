#version 450
#define M_PI 3.1415926535897932384626433832795

struct Vertex {
	vec3 pos;
	vec4 color;
};

struct Wave {
	vec3 pos;
	float outerRadius;
	float innerRadius;
	float amplitude;
	float velocity;
	float decay;
};

layout(binding = 0, rgba8) uniform readonly image2D colorTex;
layout(binding = 1, rgba8) uniform readonly image2D depthTex;

layout(std430, binding = 2) buffer vertex {
	Vertex v[];
};

layout(std430, binding = 3) buffer wave {
	Wave w[];
};

uniform float minDepth;
uniform float maxDepth;
uniform vec3 cameraPos;
uniform vec3 cameraRot;
uniform vec2 cameraFov;
uniform int waveCount;

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

// apply 5x5 convolution kernel to smooth depth edges
float getSmoothedDepth()
{
	float kernel[5][5] = {
		{ 1,  4,  6,  4, 1 },
		{ 4, 16, 24, 16, 4 },
		{ 6, 24, 36, 24, 6 },
		{ 4, 16, 24, 16, 4 },
		{ 1,  4,  6,  4, 1 },
	};
	float c = 256.f;

	float depth = 0.f;

	ivec2 maxPos = imageSize(depthTex) - 1;
	ivec2 minPos = ivec2(0, 0);
	
	for (int i = 0; i < 5; ++i)
	{
		for (int j = 0; j < 5; ++j)
		{
			ivec2 pos = ivec2(gl_GlobalInvocationID.xy) - 2 + ivec2(i, j);
			pos = clamp(pos, minPos, maxPos);
			depth += kernel[i][j] * imageLoad(depthTex, pos).r;
		}
	}

	return depth / c;
}

mat4 rotationX( in float angle ) {
	return mat4(1.0,		0,			0,			0,
			 	0, 	cos(angle),	-sin(angle),		0,
				0, 	sin(angle),	 cos(angle),		0,
				0, 			0,			  0, 		1);
}

mat4 rotationY( in float angle ) {
	return mat4(cos(angle),		0,		sin(angle),	0,
			 			0,		1.0,			 0,	0,
				-sin(angle),	0,		cos(angle),	0,
						0, 		0,				0,	1);
}

mat4 rotationZ( in float angle ) {
	return mat4(cos(angle),		-sin(angle),	0,	0,
			 	sin(angle),		cos(angle),		0,	0,
						0,				0,		1,	0,
						0,				0,		0,	1);
}

mat4 rotation( in vec3 angles ) {
	return rotationZ(angles.z) * rotationY(angles.y) * rotationX(angles.x);
}

vec3 getWorldPosition()
{
	vec2 uv = vec2(gl_GlobalInvocationID.xy) / vec2(imageSize(depthTex));

	float d = getSmoothedDepth();

	float zCam = (minDepth + d * (maxDepth - minDepth));

	vec2 focalLen = 0.5f / tan(0.5f * cameraFov);

	vec2 pixelTans = (uv - 0.5f) / focalLen;

	vec3 pos = zCam * vec3(pixelTans, 1.f);

	mat4 m = rotation(-cameraRot);

	return cameraPos + (m * vec4(pos.xyz, 1.0)).xyz;
}

void main()
{
	vec2 depthSizef = vec2(imageSize(depthTex));
	vec2 colorSizef = vec2(imageSize(colorTex));

	// Step 1: Get point in space where particle is supposed to be
	vec3 point = getWorldPosition();
	ivec2 px = ivec2(vec2(gl_GlobalInvocationID.xy) / depthSizef * colorSizef);

	// Step 2: Displace point from waves
	for (int i = 0; i < waveCount; ++i)
	{
		vec3 d = point.xyz - w[i].pos;
		float ir = w[i].innerRadius;
		float or = w[i].outerRadius;
		float r = clamp(length(d), ir, or);
		float t = (r - ir) / (or - ir);
		float c = cos(M_PI * (t - 0.5f));
		float k = c * c;

		float r2 = w[i].amplitude * k;
		point += r2 * normalize(d);
	}
	
	// Set vertex coordinate
	uint idx = 384 * gl_GlobalInvocationID.x + gl_GlobalInvocationID.y;
	v[idx].pos = point;
	v[idx].color = imageLoad(colorTex, px);
}
