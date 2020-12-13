#version 450

in vec4 fragColor;

out vec4 outColor;

uniform float multiplier16bit;

void main ()  
{  
   outColor = fragColor / multiplier16bit;
}
