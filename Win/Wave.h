#pragma once
#ifndef WAVE_H
#define WAVE_H

#include "glbinding/gl45core/gl.h"

struct Wave {
	gl::GLfloat position[4];
	gl::GLfloat displacement[4];
	gl::GLfloat color[4];
	
	gl::GLfloat blockSizeMultiplier;
	gl::GLfloat colorMix;
	gl::GLfloat outerRadius;
	gl::GLfloat innerRadius;

	gl::GLfloat timeSinceBirth;
	gl::GLfloat padding[3];
	
	Wave() : position(), displacement(), color(), blockSizeMultiplier(), colorMix(), outerRadius(), innerRadius(), timeSinceBirth(), padding() {};

	Wave(
		gl::GLfloat position[4],
		gl::GLfloat displacement[4],
		gl::GLfloat color[4],
		gl::GLfloat blockSizeMultiplier,
		gl::GLfloat colorMix,
		gl::GLfloat outerRadius,
		gl::GLfloat innerRadius,
		gl::GLfloat timeSinceBirth
	) {
		memcpy(this->position, position, 4 * sizeof(gl::GLfloat));
		memcpy(this->displacement, displacement, 4 * sizeof(gl::GLfloat));
		memcpy(this->color, color, 4 * sizeof(gl::GLfloat));

		this->blockSizeMultiplier = blockSizeMultiplier;
		this->colorMix = colorMix;
		this->outerRadius = outerRadius;
		this->innerRadius = innerRadius;

		this->timeSinceBirth = timeSinceBirth;
	};
};

#endif