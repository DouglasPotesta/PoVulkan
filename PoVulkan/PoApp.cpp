

#include "PoApp.h"
#include "PoVulkan.h"


void NPoAppBehavior::run()
{
	glfwInit();

	SPoAppSettings settings;
	SPoAppState state;
	SPoAppResources resources;

	NPoVulkanBehavior::init(resources.mVulkan, settings.mVulkan);

	state.mWindow = NPoWindowBehavior::init(resources.mWindow, settings.mWindow);
	state.mDevice = NPoVulkanDeviceBehavior::init(resources.mDevice, settings.mDevice, resources.mVulkan.mInstance, resources.mWindow.mpWindow);

	SPoWindowResizeCommand resizeCommand = {};
	resizeCommand.mpResources = &resources.mDevice;
	resizeCommand.mpSettings = &settings.mDevice;
	resizeCommand.mpState = &state.mDevice;
	glfwSetWindowUserPointer(resources.mWindow.mpWindow, &resizeCommand);

	int waiter = 600;
	SPoWindowId waiterHandle = {};
	while (!NPoWindowBehavior::should_close(resources.mWindow))
	{
		glfwPollEvents();
		NPoVulkanDeviceBehavior::draw_frame(resources.mDevice,
			state.mDevice, settings.mDevice);
		if (waiter > 0)
		{ 
			--waiter;
		}
		if (waiter == 0)
		{
			waiterHandle = NPoVulkanDeviceBehavior::add_window(resources.mDevice, state.mDevice, settings.mDevice);
			waiter = -1;
		}
		if (waiter == -1)
		{
			if (!(waiterHandle == SPoWindowId()))
			{
				if (NPoWindowBehavior::should_close(resources.mDevice.mWindowResourcesVector[0].mWindow))
				{
					vkDeviceWaitIdle(resources.mDevice.mLogicalDevice);
					NPoVulkanDeviceBehavior::close_window(resources.mDevice, state.mDevice, settings.mDevice, waiterHandle);
					waiterHandle = {};
				}
				else
				{
					NPoVulkanDeviceBehavior::draw_window_frames(resources.mDevice,
						state.mDevice, settings.mDevice);
				}
			}
		}
	}
	vkDeviceWaitIdle(resources.mDevice.mLogicalDevice);
	if (!(waiterHandle == SPoWindowId()))
	{
		NPoVulkanDeviceBehavior::close_window(resources.mDevice, state.mDevice, settings.mDevice, waiterHandle);
	}

	glfwSetWindowUserPointer(resources.mWindow.mpWindow, nullptr);

	NPoVulkanDeviceBehavior::cleanup(resources.mDevice, state.mDevice, settings.mDevice);
	
	NPoWindowBehavior::cleanup(resources.mWindow, state.mWindow, settings.mWindow);

	NPoVulkanBehavior::cleanup(resources.mVulkan, settings.mVulkan);
	
	glfwTerminate();


}
