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
	
	float blockSizeMultiplier;
	float brightness;
	float outerRadius;
	float innerRadius;
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

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

// thank you https://www.shadertoy.com/view/XljGzV
vec3 hsl2rgb( in vec3 c )
{
    vec3 rgb = clamp( abs(mod(c.x*6.0+vec3(0.0,4.0,2.0),6.0)-3.0)-1.0, 0.0, 1.0 );

    return c.z + c.y * (rgb-0.5)*(1.0-abs(2.0*c.z-1.0));
}

vec3 rgb2hsl( in vec3 c ){
  float h = 0.0;
	float s = 0.0;
	float l = 0.0;
	float r = c.r;
	float g = c.g;
	float b = c.b;
	float cMin = min( r, min( g, b ) );
	float cMax = max( r, max( g, b ) );

	l = ( cMax + cMin ) / 2.0;
	if ( cMax > cMin ) {
		float cDelta = cMax - cMin;
        
        //s = l < .05 ? cDelta / ( cMax + cMin ) : cDelta / ( 2.0 - ( cMax + cMin ) ); Original
		s = l < .0 ? cDelta / ( cMax + cMin ) : cDelta / ( 2.0 - ( cMax + cMin ) );
        
		if ( r == cMax ) {
			h = ( g - b ) / cDelta;
		} else if ( g == cMax ) {
			h = 2.0 + ( b - r ) / cDelta;
		} else {
			h = 4.0 + ( r - g ) / cDelta;
		}

		if ( h < 0.0) {
			h += 6.0;
		}
		h = h / 6.0;
	}
	return vec3( h, s, l );
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
	
	float brightness = 0.f;
	float size = 1.f;
	float depth = length(point);

	

	float m = (farBlockSize - nearBlockSize) / (maxDepth - minDepth);
	float b = farBlockSize - m * maxDepth;
	float blockSize = m * depth + b;

	// Step 2: Displace point from waves
	for (int i = 0; i < waveCount; ++i)
	{
		vec3 d = point.xyz - w[i].position.xyz;
		float ir = w[i].innerRadius;
		float or = w[i].outerRadius;
		float r = clamp(length(d), ir, or);
		float t = (r - ir) / (or - ir);
		float c = cos(M_PI * (t - 0.5f));
		float k = c * c;

		if (length(w[i].displacement.xyz) < 0.01) {
			point += k * w[i].displacement.w * normalize(d);
		} else {
			point += k * w[i].displacement.w * normalize(w[i].displacement.xyz);
		}

		brightness += k * w[i].brightness;

		size *= mix(1.0, w[i].blockSizeMultiplier, k);
	}

	size *= blockSize;
	
	// Set vertex coordinate
	uint idx = gl_NumWorkGroups.y * gl_GlobalInvocationID.x + gl_GlobalInvocationID.y;
	v[idx].pos = vec4(point, 1.0);

	// Set vertex color
	vec3 rgb;
	vec4 pixelColor = imageLoad(colorTex, px);

	if (waveCount == 0) {
		v[idx].color = pixelColor;
		v[idx].size.x = blockSize;
	} else {
		vec3 hsl = rgb2hsl(pixelColor.gba);
		// Normalize and clamp brightness to [-1, 1]
		float b = clamp(brightness, -1.f, 1.f); 
		// If hsl.z > 0, lighten from hsl.z to 1.0, otherwise darken from 0.0 to hsl.z
		float l = b > 0.f ? mix(hsl.z, 1.0, b) : mix(hsl.z, 0, -b);
		vec3 rgb = hsl2rgb(vec3(hsl.xy, l));
		v[idx].color = vec4(1.0, rgb.r, rgb.g, rgb.b);
		v[idx].size = vec4(size);
	}
}
