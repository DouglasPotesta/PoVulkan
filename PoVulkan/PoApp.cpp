

#include "PoApp.h"
#include "PoVulkan.h"
#include <thread>
#include <mutex>
#include <chrono>
#include <random>

namespace NPoAppPrivate
{
	void sync_render_thread();

	void sync_game_thread();

	template <typename t>
	t get_random_t()
	{
		return static_cast<t>(std::rand() / static_cast<t>(RAND_MAX));
	}

	float vec3_dot(glm::vec3 const &a, glm::vec3 const &b)
	{
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	float vec3_magnitude(glm::vec3 const &v)
	{
		return sqrtf(vec3_dot(v, v));
	}

	void run_game_loop(SPoAppState &state, SPoAppResources &resources, std::atomic_bool &gameBusy, std::atomic_bool const &renderBusy, std::atomic_bool const &shouldExit, SPoAppSettings const &settings)
	{
		srand(static_cast<unsigned int>(time(nullptr)));
		std::chrono::time_point startTime = std::chrono::high_resolution_clock::now();
		float lastRelativeTime = 0.0f;
		while (!shouldExit)
		{
			//SyncRenderThread();
			while (renderBusy)
			{
				std::this_thread::yield();
			}

			gameBusy = true;
			std::chrono::time_point const currentTime = std::chrono::high_resolution_clock::now();
			float relativeTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
			float deltaTime = relativeTime - lastRelativeTime;
			lastRelativeTime = relativeTime;
			float restLength = 0.5;
			/*ProcessGameLogic();*/
			//std::array<SPoGameObjectState, NPoGameObjectBehavior::gk_max_game_objects> &gos = state.mDevice.mGameObjects;
			//for (int i = 0; i < gos.size(); ++i)
			//{
			//	for (int j = i + 1; j < gos.size(); ++j)
			//	{
			//		SPoGameObjectState &a = gos[i];
			//		SPoGameObjectState &b = gos[j];
			//		glm::vec3 bToA = a.mTransform.mPosition - b.mTransform.mPosition;
			//		float distance = vec3_magnitude(bToA);
			//		if (distance < 0.0001f)
			//		{
			//			bToA = { get_random_t<float>(), get_random_t<float>(), get_random_t<float>() };
			//			bToA *= 10.0f;
			//			distance = vec3_magnitude(bToA);
			//		}
			//		float k = 100.5;
			//		float force = -k * (distance - restLength);
			//		glm::vec3 velocityDelta = bToA * (force * deltaTime * 0.5f / distance);
			//		a.mVelocity += velocityDelta;
			//		b.mVelocity -= velocityDelta;
			//	}
			//}
			//for (int i = 0; i < gos.size(); ++i)
			//{
			//	SPoGameObjectState &a = gos[i];
			//	a.mTransform.mPosition += a.mVelocity * deltaTime;
			//}
			gameBusy = false;
			while (renderBusy)
			{
				std::this_thread::yield();
			}
			//SyncRenderThread();
		}
		gameBusy = false;
	}
}


void NPoAppBehavior::run()
{
	glfwInit();

	SPoAppSettings settings;
	SPoAppState state;
	SPoAppResources resources;

	std::atomic_bool shouldExit = false;
	std::mutex syncMutex;
	std::atomic_bool renderBusy = true;
	std::atomic_bool gameBusy = false;

	std::thread gameThread(NPoAppPrivate::run_game_loop, std::ref(state), std::ref(resources), std::ref(gameBusy), std::ref(renderBusy), std::ref(shouldExit), std::ref(settings));

	state.mVulkan = NPoVulkanBehavior::init(resources.mVulkan, settings.mVulkan);

	state.mWindows.push_back(NPoWindowBehavior::init(resources.mWindows.emplace_back(), settings.mWindow));
	GLFWwindow *pWindow = resources.mWindows.back().mpWindow;
	state.mDevice = NPoVulkanDeviceBehavior::init(resources.mDevice, settings.mDevice, resources.mVulkan.mInstance, pWindow);
	
	SPoWindowResizeCommand resizeCommand = {};
	resizeCommand.mpResources = &resources.mDevice;
	resizeCommand.mpSettings = &settings.mDevice;
	resizeCommand.mpState = &state.mDevice;
	glfwSetWindowUserPointer(pWindow, &resizeCommand);

	
	auto getPrimaryWindowFunc = [&]() -> SPoWindowResources &
		{
			for (int i = 0; i < resources.mWindows.size(); ++i)
			{
				if (resources.mWindows[i].mpWindow == pWindow)
				{
					return resources.mWindows[i];
				}
			}
			throw std::runtime_error("No primary window found!");
		};

	int waiter = 600;
	SPoWindowId waiterHandle = {};
	
	while (shouldExit == false)
	{
		glfwPollEvents();
		while (gameBusy)
		{
			std::this_thread::yield();
		}
		renderBusy = true;
		--waiter;
		if (waiter == 0)
		{
			waiter = 100;
			state.mWindows.push_back(NPoWindowBehavior::init(resources.mWindows.emplace_back(), settings.mWindow));
			NPoVulkanDeviceBehavior::add_window(resources.mDevice, state.mDevice, settings.mDevice, resources.mWindows.back().mpWindow);
		}
		for (size_t windowIndex = resources.mWindows.size(); windowIndex > 0; )
		{
			--windowIndex;
			bool const shouldClose = NPoWindowBehavior::should_close(resources.mWindows[windowIndex]);

			shouldExit = shouldExit || (shouldClose && pWindow == resources.mWindows[windowIndex].mpWindow);
			if (shouldExit)
			{
				break;
			}
			if (shouldClose)
			{
				vkDeviceWaitIdle(resources.mDevice.mLogicalDevice);
				NPoVulkanDeviceBehavior::close_window(resources.mDevice, state.mDevice, settings.mDevice, resources.mWindows[windowIndex].mpWindow);
				NPoWindowBehavior::cleanup(resources.mWindows[windowIndex], state.mWindows[windowIndex], settings.mWindow);
				size_t const endIndex = resources.mWindows.size() - 1;
				resources.mWindows[windowIndex] = std::move(resources.mWindows[endIndex]);
				state.mWindows[windowIndex] = std::move(state.mWindows[endIndex]);
				resources.mWindows.pop_back();
				state.mWindows.pop_back();
				waiterHandle = {};
			}
		}
		if (!shouldExit)
		{
			NPoVulkanDeviceBehavior::draw_window_frames(resources.mDevice, state.mDevice, settings.mDevice);
		}
		renderBusy = false;
		while (gameBusy)
		{
			std::this_thread::yield();
		}
	}
	gameThread.join();
	vkDeviceWaitIdle(resources.mDevice.mLogicalDevice);

	glfwSetWindowUserPointer(pWindow, nullptr);

	NPoVulkanDeviceBehavior::cleanup(resources.mDevice, state.mDevice, settings.mDevice);
	
	for (size_t i = resources.mWindows.size(); i > 0;)
	{
		--i;
		NPoWindowBehavior::cleanup(resources.mWindows[i], state.mWindows[i], settings.mWindow);
	}

	NPoVulkanBehavior::cleanup(resources.mVulkan, state.mVulkan, settings.mVulkan);

	glfwTerminate();


}
