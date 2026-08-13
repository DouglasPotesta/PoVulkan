#pragma once

#include "PoTransform.h"


struct SPoCameraState
{
	SPoTransform mTransform = {};
	float mFOV = 90.0f;
	glm::vec2 mViewportPosition = { 0.0, 0.0 };
	glm::vec2 mViewportCoverage = { 1.0, 1.0 };

};
