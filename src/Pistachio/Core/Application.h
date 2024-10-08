#pragma once

#include "Pistachio/Core.h"
#include "Pistachio/Core/LayerStack.h"
#include "Pistachio/Event/Event.h"
#include "Pistachio/Core/Window.h"
#include "Pistachio/Event/KeyEvent.h"

#include "Pistachio/Renderer/Renderer.h"
#include "Pistachio/Renderer/Buffer.h"
#include "Pistachio/Renderer/RendererBase.h"
#include "Pistachio/Renderer/Shader.h"
#include "Pistachio/Renderer/Camera.h"
#include "Pistachio/Core/Input.h"
#include "Pistachio/Asset/AssetManager.h"
#include <memory>
namespace Pistachio {

	/*
	* Describes configuration for Pistachio Application
	* - Headless: Create Window for application (if yes, scenes would still render to textures)
	* - GPU LUID: if not zero, the specific gpu to use on creation
	*/
	struct PISTACHIO_API ApplicationOptions
	{
		bool headless = false;
		RHI::LUID gpu_luid{};
		bool exportTextures = false;
		bool forceSingleQueue;
		//using custom devices
		Internal_ID custom_device = nullptr;
		Internal_ID custom_instance = nullptr;
		Internal_ID custom_physical_device = nullptr;
		Internal_ID custom_direct_queue = nullptr;//required
		Internal_ID custom_compute_queue = nullptr;//optional
		RHI::QueueFamilyIndices indices;
		//logger options
		//leave null to log to stdout
		const char* log_file_name = nullptr;
		std::string_view shader_dir;
		std::function<RHI::PhysicalDevice*(std::span<RHI::PhysicalDevice*>)> select_physical_device;
	};
	class PISTACHIO_API Application
	{
	public:
		explicit Application(const char* name, const ApplicationOptions& options = ApplicationOptions());
		virtual ~Application();
		void Run();
		void Step();
		void OnEvent(Event& e);
		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlay);
		bool OnWindowResize(WindowResizeEvent& e);
		void SetInputHandler(std::unique_ptr<InputHandler> handler);
		[[nodiscard]] RendererBase& GetRendererBase() {return *m_rendererBase;}
		[[nodiscard]] Renderer& GetRenderer() {return *m_renderer;}
		[[nodiscard]] InputHandler& GetInputHandler() {return *handler;}
		[[nodiscard]] AssetManager& GetAssetManager() {return *m_assetManager;}
		static Application& Get();
		static bool Exists();
		[[nodiscard]] Window* GetWindow() { return m_Window.get(); }
		void SetImGuiContext(void* ctx);
		void Stop() {m_Running = false;}
		[[nodiscard]] bool IsHeadless() const {
			return m_headless;
		};
		const std::string& GetShaderDir() {return shaderDir;}
	private:
		bool m_headless = false;
		std::unique_ptr<RendererBase> m_rendererBase;
		std::unique_ptr<AssetManager> m_assetManager;
		std::unique_ptr<Renderer> m_renderer;
		std::unique_ptr<Window> m_Window;
		std::unique_ptr<InputHandler> handler;
		LayerStack m_layerstack;
		bool m_Running = true;
		bool m_minimized = false;
		static Application* s_Instance;
		std::string shaderDir;
		std::chrono::time_point<std::chrono::high_resolution_clock> InitTime;
		std::chrono::milliseconds lastFrameTime;
		//LARGE_INTEGER frequency;
		double period;
		//LARGE_INTEGER ticks;
	};
	
	Application* CreateApplication();
}
