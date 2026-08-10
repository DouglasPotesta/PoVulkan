#pragma once

#include "PoGlfw.h"

#include <string>
#include <memory>
#include <memory>

struct SPoWindowSettings
{
	int mWidth = 800;
	int mHeight = 600;
	std::string mName = "PoVulkan";
};


struct SPoWindowResources
{
	GLFWwindow *mpWindow = nullptr;
};

struct SPoWindowState
{
};

namespace NPoWindowBehavior
{
	SPoWindowState init(SPoWindowResources &inOutResources, SPoWindowSettings const &settings);
	void cleanup(SPoWindowResources &inOutResources, SPoWindowState &inOutState, SPoWindowSettings const &settings);
	inline bool should_close(SPoWindowResources const &resources)
	{
		return glfwWindowShouldClose(resources.mpWindow);
	}

}