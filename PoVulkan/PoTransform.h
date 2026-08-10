#pragma once
#include "PoGlm.h"
struct SPoTransform
{
	glm::vec3 mPosition = { 0.0f, 0.0f, 0.0f };
	glm::vec3 mRotation = { 0.0f, 0.0f, 0.0f };
	glm::vec3 mScale = { 1.0f, 1.0f, 1.0f };


	inline glm::mat4 ToMatrix() const
	{
		glm::mat4 model = glm::mat4(1.0);
		model = glm::translate(model, mPosition);
		model = glm::rotate(model, mRotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, mRotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, mRotation.z, glm::vec3(1.0f, 0.0f, 1.0f));
		model = glm::scale(model, mScale);
		return model;
	}
};
