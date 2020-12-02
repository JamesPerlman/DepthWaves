#version 450
#extension GL_EXT_geometry_shader4 : enable

layout (points) in;
layout (triangle_strip, max_vertices = 14) out;

in vec4 vertColor[];

out vec4 fragColor;

uniform float size;

uniform mat4 modelViewProjectionMatrix;

const float cube[42] = {
    -1.f, 1.f, 1.f,     // Front-top-left
    1.f, 1.f, 1.f,      // Front-top-right
    -1.f, -1.f, 1.f,    // Front-bottom-left
    1.f, -1.f, 1.f,     // Front-bottom-right
    1.f, -1.f, -1.f,    // Back-bottom-right
    1.f, 1.f, 1.f,      // Front-top-right
    1.f, 1.f, -1.f,     // Back-top-right
    -1.f, 1.f, 1.f,     // Front-top-left
    -1.f, 1.f, -1.f,    // Back-top-left
    -1.f, -1.f, 1.f,    // Front-bottom-left
    -1.f, -1.f, -1.f,   // Back-bottom-left
    1.f, -1.f, -1.f,    // Back-bottom-right
    -1.f, 1.f, -1.f,    // Back-top-left
    1.f, 1.f, -1.f      // Back-top-right
};

void main() {
	vec4 center = gl_in[0].gl_Position;

	fragColor = vertColor[0];

	for (int i = 0; i < 14; ++i)
	{
		vec3 p = vec3(cube[3 * i], cube[3 * i + 1], cube[3 * i + 2]) * size;
		gl_Position = modelViewProjectionMatrix * (center + vec4(p, 1.0));
		EmitVertex();
	}

	EndPrimitive();
}
