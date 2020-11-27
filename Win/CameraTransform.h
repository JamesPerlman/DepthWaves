#include "gl_plus.h"

namespace {
	typedef struct CameraTransform {
		gl::GLvec3f rotation;
		gl::GLvec3f position;
		gl::GLvec2f fov;

		CameraTransform(gl::GLvec3f _pos, gl::GLvec3f _rot, gl::GLvec2f _fov)
			: position(_pos)
			, rotation(_rot)
			, fov(_fov) {};
	} CameraTransform;
}
