

#include "PoApp.h"
#include "PoVulkan.h"


void NPoAppBehavior::run()
{
	glfwInit();

	SPoAppSettings settings;
	SPoAppState state;
	SPoAppResources resources;

	NPoVulkanBehavior::init(resources.mVulkan, settings.mVulkan);

	state.mWindow = NPoWindowBehavior::init(resources.mWindow, settings.mWindow, resources.mVulkan.mInstance);

	SPoWindowResizeCommand resizeCommand = {};
	resizeCommand.mpResources = &resources.mWindow;
	resizeCommand.mpSettings = &settings.mWindow;
	resizeCommand.mpState = &state.mWindow;
	glfwSetWindowUserPointer(resources.mWindow.mpWindow, &resizeCommand);
	while (!NPoWindowBehavior::should_close(resources.mWindow))
	{
		glfwPollEvents();
	}
	glfwSetWindowUserPointer(resources.mWindow.mpWindow, nullptr);
	NPoWindowBehavior::cleanup(resources.mWindow, state.mWindow, settings.mWindow);

	NPoVulkanBehavior::cleanup(resources.mVulkan, settings.mVulkan);
	
	glfwTerminate();


}
