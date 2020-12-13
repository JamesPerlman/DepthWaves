#pragma once
#ifndef WAVE_H
#define WAVE_H

#include "glbinding/gl45core/gl.h"

struct Wave {
	gl::GLfloat position[4];
	gl::GLfloat displacement[4];
	
	gl::GLfloat blockSizeMultiplier;
	gl::GLfloat brightness;
	gl::GLfloat outerRadius;
	gl::GLfloat innerRadius;
	
	Wave() : position(), displacement(), blockSizeMultiplier(), brightness(), outerRadius(), innerRadius() {};

	Wave(
		gl::GLfloat position[4],
		gl::GLfloat displacement[4],
		gl::GLfloat blockSizeMultiplier,
		gl::GLfloat brightness,
		gl::GLfloat outerRadius,
		gl::GLfloat innerRadius
	) {
		memcpy(this->position, position, 4 * sizeof(gl::GLfloat));
		memcpy(this->displacement, displacement, 4 * sizeof(gl::GLfloat));
		this->blockSizeMultiplier = blockSizeMultiplier;
		this->brightness = brightness;
		this->outerRadius = outerRadius;
		this->innerRadius = innerRadius;
	};
};

#endif