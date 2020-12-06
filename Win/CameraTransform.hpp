#pragma once
#ifndef CameraTransform_H
#define CameraTransform_H

#include "../vmath.hpp"
#include "glbinding/gl45core/gl.h"


namespace {


	vmath::Matrix4 rotationX(gl::GLfloat angle) {
		return vmath::Matrix4(
			vmath::Vector4(1.f,			 0.f,		  0.f, 0.f),
			vmath::Vector4(0.f,  cosf(angle), sinf(angle), 0.f),
			vmath::Vector4(0.f, -sinf(angle), cosf(angle), 0.f),
			vmath::Vector4(0.f,			 0.f,		  0.f, 1.f)
		);
	}

	vmath::Matrix4 rotationY(gl::GLfloat angle) {
		return vmath::Matrix4(
			vmath::Vector4(cosf(angle),			 0.f, -sinf(angle),     0.f),
			vmath::Vector4(		   0.f,			 1.f,		   0.f,		0.f),
			vmath::Vector4(sinf(angle),			 0.f,  cosf(angle),	    0.f),
			vmath::Vector4(		   0.f,			 0.f,		   0.f,		1.f)
		);
	}

	vmath::Matrix4 rotationZ(gl::GLfloat angle) {
		return vmath::Matrix4(
			vmath::Vector4( cosf(angle),  sinf(angle),		0.f,    0.f),
			vmath::Vector4(-sinf(angle),  cosf(angle),		0.f,	0.f),
			vmath::Vector4(			0.f,		  0.f,		1.f,	0.f),
			vmath::Vector4(		    0.f,		  0.f,		0.f,	1.f)
		);
	}

	vmath::Matrix4 rotation(vmath::Vector3 angles) {
		return rotationZ(angles.getZ()) * rotationY(angles.getY()) * rotationX(angles.getX());
	}

	typedef struct CameraTransform {
		vmath::Vector3 fov;
		vmath::Matrix4 projectionMatrix;

		CameraTransform(vmath::Vector3 fov, float focalLen, float clipNear, float clipFar)
		{
			float n = clipNear;
			float f = clipFar;
			float r = n * tanf(0.5f * fov.getX()); // right
			float t = n * tanf(0.5f * fov.getY()); // top

			float k1 = -(f + n) / (f - n);
			float k2 = -(2.f * f * n) / (f - n);
			this->projectionMatrix = vmath::Matrix4(
				vmath::Vector4(n / r, 0.f, 0.f, 0.f),
				vmath::Vector4(0.f, n / t, 0.f, 0.f),
				vmath::Vector4(0.f, 0.f, k1, k2),
				vmath::Vector4(0.f, 0.f, -1.f, 0.f)
			);
			this->fov = fov;
		};
	} CameraTransform;
}

#endif