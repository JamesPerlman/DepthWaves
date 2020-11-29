#version 440
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec4 inVertex;
layout (location = 4) in vec4 inColor;

out vec4 vertColor;

void main()
{
	gl_Position = inVertex;
	vertColor = inColor;
}
