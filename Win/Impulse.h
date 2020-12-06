#pragma once
#ifndef IMPULSE_H
#define IMPULSE_H

#include "AE_Effect.h"
#include "../vmath.hpp"

namespace {
	struct Impulse {
		A_long startTime, endTime;
		vmath::Vector3 position;
		Impulse(vmath::Vector3 position, A_long startTime, A_long endTime)
			: position(position)
			, startTime(startTime)
			, endTime(endTime)
		{}
	};
}

#endif IMPULSE_H