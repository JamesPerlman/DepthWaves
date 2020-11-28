#version 440
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 inVertex;
layout (location = 3) in vec4 inColor;

out vec4 vertColor;

void main()
{
	gl_Position = vec4(inVertex, 1.0);
	vertColor = inColor;
}
