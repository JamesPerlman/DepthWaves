#pragma once
#include "glbinding/gl45core/gl.h"

namespace gl {
	struct GLvec3f {
		GLfloat x, y, z;
		GLvec3f(GLfloat _x, GLfloat _y, GLfloat _z)
			: x(_x), y(_y), z(_z) {};
	};

	struct GLvec2f {
		GLfloat x, y;
		GLvec2f(GLfloat _x, GLfloat _y)
			: x(_x), y(_y) {};
	};
}