
#include "PoWindow.h"



SPoWindowState NPoWindowBehavior::init(SPoWindowResources &inOutResources, SPoWindowSettings const &settings)
{
	SPoWindowState state;
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	inOutResources.mpWindow = glfwCreateWindow(settings.mWidth, settings.mHeight, settings.mName.c_str(), nullptr, nullptr);

	return state;
}

void NPoWindowBehavior::cleanup(SPoWindowResources &inOutResources, SPoWindowState &inOutState, SPoWindowSettings const &settings)
{
	glfwDestroyWindow(inOutResources.mpWindow);
	inOutResources.mpWindow = nullptr;
}