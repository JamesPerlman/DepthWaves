#pragma once
#ifndef IMPULSE_H
#define IMPULSE_H

#include "AE_Effect.h"
#include "../vmath.hpp"

namespace {
	struct Impulse {
		A_long startTime, endTime;
		Impulse(A_long startTime, A_long endTime)
			: startTime(startTime)
			, endTime(endTime)
		{}
	};
}

#endif IMPULSE_H