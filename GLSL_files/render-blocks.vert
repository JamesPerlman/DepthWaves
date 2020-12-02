#version 450

layout (location = 0) in vec4 inVertex;
layout (location = 1) in vec4 inColor;

out vec4 vertColor;

void main()
{
	gl_Position = inVertex;
	vertColor = inColor;
}
