#version 450
#define M_PI 3.1415926535897932384626433832795

vec2 uv = vec2(gl_GlobalInvocationID.xy) / vec2(gl_NumWorkGroups.xy);

struct Vertex {
	vec4 pos;
	vec4 color;
	vec4 size;
};

struct Wave {
	vec4 position;
	vec4 displacement;
	vec4 color;
	
	float blockSizeMultiplier;
	float colorMix;
	float outerRadius;
	float innerRadius;

	vec4 timeSinceBirth; // timeSinceBirth should be x
};

layout(binding = 0, rgba32f) uniform readonly image2D colorTex;
layout(binding = 1, rgba32f) uniform readonly image2D depthTex;

layout(std430, binding = 2) buffer vertex {
	Vertex v[];
};

layout(std430, binding = 3) buffer wave {
	Wave w[];
};

uniform float minDepth;
uniform float maxDepth;
uniform vec2 cameraFov;
uniform int waveCount;
uniform float nearBlockSize;
uniform float farBlockSize;
uniform bool colorizeWaves;
uniform float colorCycleRadius;

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

vec3 hsl2rgb(vec3 HSL)
{
  float R = abs(HSL.x * 6.0 - 3.0) - 1.0;
  float G = 2.0 - abs(HSL.x * 6.0 - 2.0);
  float B = 2.0 - abs(HSL.x * 6.0 - 4.0);
  vec3 RGB = clamp(vec3(R,G,B), 0.0, 1.0);
  float C = (1.0 - abs(2.0 * HSL.z - 1.0)) * HSL.y;
  return (RGB - 0.5) * C + HSL.z;
}

vec3 rgb2hsl(vec3 color) {
 	vec3 hsl; // init to 0 to avoid warnings ? (and reverse if + remove first part)

 	float fmin = min(min(color.r, color.g), color.b); //Min. value of RGB
 	float fmax = max(max(color.r, color.g), color.b); //Max. value of RGB
 	float delta = fmax - fmin; //Delta RGB value

 	hsl.z = (fmax + fmin) / 2.0; // Luminance

 	if (delta == 0.0) //This is a gray, no chroma...
 	{
 		hsl.x = 0.0; // Hue
 		hsl.y = 0.0; // Saturation
 	} else //Chromatic data...
 	{
 		if (hsl.z < 0.5)
 			hsl.y = delta / (fmax + fmin); // Saturation
 		else
 			hsl.y = delta / (2.0 - fmax - fmin); // Saturation

 		float deltaR = (((fmax - color.r) / 6.0) + (delta / 2.0)) / delta;
 		float deltaG = (((fmax - color.g) / 6.0) + (delta / 2.0)) / delta;
 		float deltaB = (((fmax - color.b) / 6.0) + (delta / 2.0)) / delta;

 		if (color.r == fmax)
 			hsl.x = deltaB - deltaG; // Hue
 		else if (color.g == fmax)
 			hsl.x = (1.0 / 3.0) + deltaR - deltaB; // Hue
 		else if (color.b == fmax)
 			hsl.x = (2.0 / 3.0) + deltaG - deltaR; // Hue

 		if (hsl.x < 0.0)
 			hsl.x += 1.0; // Hue
 		else if (hsl.x > 1.0)
 			hsl.x -= 1.0; // Hue
 	}

 	return hsl;
 }

float getDepth(ivec2 inPos) {

	return imageLoad(depthTex, inPos).g;
}

// apply 5x5 convolution kernel to smooth depth edges
float getSmoothedDepth(ivec2 inPos)
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
			ivec2 pos = inPos - 2 + ivec2(i, j);
			pos = clamp(pos, minPos, maxPos);
			depth += kernel[i][j] * getDepth(pos);
		}
	}

	return depth / c;
}

vec3 getWorldPosition()
{
	ivec2 depthImageSize = ivec2(imageSize(depthTex));

	float d = getDepth(ivec2(uv * vec2(depthImageSize)));

	float zCam = -(maxDepth + d * (minDepth - maxDepth));

	vec2 focalLen = 0.5f / tan(0.5f * cameraFov);

	vec2 pixelTans = (uv - 0.5f) / focalLen;

	vec3 pos = zCam * vec3(pixelTans, 1.f);

	return vec4(-pos.xy, pos.z, 1.0).xyz;
}

void main()
{
	vec2 depthSizef = vec2(imageSize(depthTex));
	vec2 colorSizef = vec2(imageSize(colorTex));
	
	// Step 1: Get point in space where particle is supposed to be
	ivec2 px = ivec2(uv * colorSizef);
	vec3 point = getWorldPosition();
	
	vec4 pixelColor = imageLoad(colorTex, px).gbar;
	vec4 blockColor = pixelColor;
	float size = 1.f;
	float depth = length(point);

	float m = (farBlockSize - nearBlockSize) / (maxDepth - minDepth);
	float b = farBlockSize - m * maxDepth;
	float blockSize = m * depth + b;

	// Step 2: Displace point from waves
	for (int i = 0; i < waveCount; ++i)
	{
		vec3 d = point.xyz - w[i].position.xyz;
		float lc = length(d);
		float ir = w[i].innerRadius;
		float or = w[i].outerRadius;
		float r = clamp(lc, ir, or);
		float t = (r - ir) / (or - ir);
		float c = cos(M_PI * (t - 0.5f));
		float k = c * c;

		if (length(w[i].displacement.xyz) < 0.01) {
			point += k * w[i].displacement.w * normalize(d);
		} else {
			point += k * w[i].displacement.w * normalize(w[i].displacement.xyz);
		}

		vec4 targetColor;
		if (colorizeWaves) {
			vec3 hsl = rgb2hsl(w[i].color.rgb);
			float hue = mod(lc + hsl.z, colorCycleRadius) / colorCycleRadius;
			vec3 rgb = hsl2rgb(vec3(hue, hsl.y, hsl.z));
			targetColor = mix(pixelColor, vec4(rgb, 1.0), w[i].colorMix);
		} else {
			targetColor = mix(pixelColor, w[i].color, w[i].colorMix);
		}
		blockColor = mix(blockColor, targetColor, k);

		size *= mix(1.0, w[i].blockSizeMultiplier, k);
	}

	size *= blockSize;
	
	// Set vertex coordinate
	uint idx = gl_NumWorkGroups.y * gl_GlobalInvocationID.x + gl_GlobalInvocationID.y;
	v[idx].pos = vec4(point, 1.0);

	if (waveCount == 0) {
		v[idx].color = pixelColor.argb;
		v[idx].size.x = blockSize;
	} else {
		v[idx].color = blockColor.argb;
		v[idx].size = vec4(size);
	}
}
