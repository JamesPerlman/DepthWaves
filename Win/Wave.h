#pragma once
#ifndef WAVE_H
#define WAVE_H

#include "glbinding/gl45core/gl.h"

struct Wave {
	gl::GLfloat pos[4];
	gl::GLfloat outerRadius, innerRadius;
	gl::GLfloat padding[2];

	Wave() : pos(), outerRadius(), innerRadius(), padding() {};

	Wave(gl::GLfloat pos[4], gl::GLfloat outerRadius, gl::GLfloat innerRadius)
	{
		memcpy(this->pos, pos, 4 * sizeof(gl::GLfloat));
		this->outerRadius = outerRadius;
		this->innerRadius = innerRadius;
	};
};

#endif