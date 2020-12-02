#include "../vmath.hpp"

namespace {
	typedef struct CameraTransform {
		vmath::Vector3 rotation;
		vmath::Vector3 position;
		vmath::Vector3 fov;
		vmath::Matrix4 projectionMatrix;

		CameraTransform(vmath::Vector3 position, vmath::Vector3 rotation, vmath::Vector3 fov, float focalLen, float clipNear, float clipFar)
			: position(position)
			, rotation(rotation)
			, fov(fov)
		{
			float n = clipNear;
			float f = clipFar;
			float l = -focalLen * sinf(0.5f * fov.getX()); // left
			float r = -l; // right
			float t = focalLen * sinf(0.5f * fov.getY()); // top
			float b = -t; // bottom

			float k1 = (f + n) / (n - f);
			float k2 = (2.f * f * n) / (n - f);
			this->projectionMatrix = vmath::Matrix4(
				vmath::Vector4(n / r, 0.f, 0.f, 0.f),
				vmath::Vector4(0.f, n / t, 0.f, 0.f),
				vmath::Vector4(0.f, 0.f, k1, -1.f),
				vmath::Vector4(0.f, 0.f, k2, 0.f)
			);
		};
	} CameraTransform;
}
