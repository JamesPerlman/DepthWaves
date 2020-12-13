#version 450

layout (location = 0) in vec4 inVertex;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec4 inSize;

out vec4 vertColor;
out vec4 vertSize;

void main()
{
	gl_Position = inVertex;
	vertColor = inColor;
	vertSize = inSize;
}
