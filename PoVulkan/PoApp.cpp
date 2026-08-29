

#include "PoApp.h"
#include "PoVulkan.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
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

	void tickgui()
	{

		static bool my_tool_active = true;
		static float my_color[4] = { 1.0, 0.0, 1.0, 1.0 };
		ImGuiIO &io = ImGui::GetIO();
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		if (my_tool_active)
		{
			// Create a window called "My First Tool", with a menu bar.
			ImGui::Begin("My First Tool", &my_tool_active, ImGuiWindowFlags_MenuBar);
			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("Open..", "Ctrl+O")) { /* Do stuff */ }
					if (ImGui::MenuItem("Save", "Ctrl+S")) { /* Do stuff */ }
					if (ImGui::MenuItem("Close", "Ctrl+W")) { my_tool_active = false; }
					ImGui::EndMenu();
				}
				ImGui::EndMenuBar();
			}

			// Edit a color stored as 4 floats
			ImGui::ColorEdit4("Color", my_color);

			// Generate samples and plot them
			float samples[100];
			for (int n = 0; n < 100; n++)
				samples[n] = sinf(n * 0.2f + static_cast<float>(ImGui::GetTime()) * 1.5f);
			ImGui::PlotLines("Samples", samples, 100);

			// Display contents in a scrolling region
			ImGui::TextColored(ImVec4(1, 1, 0, 1), "Important Stuff");
			ImGui::BeginChild("Scrolling");
			for (int n = 0; n < 50; n++)
				ImGui::Text("%04d: Some text", n);
			ImGui::EndChild();
			ImGui::End();
		}
		ImGui::Render();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			// TODO for OpenGL: restore current GL context.
		}
	}

	void run_game_loop(SPoAppState &state, SPoAppResources &resources,
		std::atomic_bool &gameBusy, std::atomic_bool const &renderBusy, std::atomic_bool const &shouldExit,
		SPoAppSettings const &settings)
	{
		srand(static_cast<unsigned int>(time(nullptr)));
		std::chrono::time_point startTime = std::chrono::high_resolution_clock::now();
		float lastRelativeTime = 0.0f;
		while (!shouldExit)
		{
			// run game logic in parallel with render thread
			std::chrono::time_point const currentTime = std::chrono::high_resolution_clock::now();
			float relativeTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
			float deltaTime = relativeTime - lastRelativeTime;
			lastRelativeTime = relativeTime;
			float restLength = 0.5;

			for (int i = 0; i < state.mGames.size(); ++i)
			{
				SPoGameState &gameState = state.mGames[i];

				// tick game state

			}

			// Sync the game thread and render thread
			while (renderBusy.load(std::memory_order_acquire))
			{
				std::this_thread::yield();
			}
			gameBusy.store(true, std::memory_order_release);

			for (int i = 0; i < state.mGames.size(); ++i)
			{
				SPoGameState const &gameState = state.mGames[i];
				SPoWindowState &windowState = state.mWindows[i];
				// copy game state into window state for rendering
			}

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




			gameBusy.store(false, std::memory_order_release);
			while (renderBusy.load(std::memory_order_acquire))
			{
				std::this_thread::yield();
			}
			//SyncRenderThread();
		}
		gameBusy.store(false, std::memory_order_release);
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
	std::atomic_bool renderBusy = false;
	std::atomic_bool gameBusy = true;

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
	std::thread gameThread(NPoAppPrivate::run_game_loop, std::ref(state), std::ref(resources), std::ref(gameBusy), std::ref(renderBusy), std::ref(shouldExit), std::ref(settings));

	int frame = 0;
	while (shouldExit == false)
	{
		glfwPollEvents();
		while (gameBusy.load(std::memory_order_acquire))
		{
			std::this_thread::yield();
		}
		renderBusy.store(true, std::memory_order_release);
		if (frame++ < 5)
		{
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
			}
		}
		if (!shouldExit)
		{
			for (int i = 0; i < state.mDevice.mWindowStateVector.size(); ++i)
			{
				ImGui::SetCurrentContext(resources.mDevice.mWindowResourcesVector[i].mGui.mpContext);
				NPoAppPrivate::tickgui();
			}
			// we have two options. I think the sanest thing is to...
			// have every window cache it's imgui commands and then flush each batch at render
			NPoVulkanDeviceBehavior::draw_window_frames(resources.mDevice, state.mDevice, settings.mDevice);
		}

		renderBusy.store(false, std::memory_order_release);
		while (gameBusy.load(std::memory_order_acquire))
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
