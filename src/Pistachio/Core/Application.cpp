#include "Pistachio/Core/Window.h"
#include "Pistachio/Renderer/RendererBase.h"
#include "Pistachio/Core/Log.h"
#include "Pistachio/Core/Application.h"
#include "Pistachio/Event/Event.h"
#include "Pistachio/Core/Input.h"
#include "Pistachio/Renderer/Renderer2D.h"
#include "imgui.h"
#include "tracy/Tracy.hpp"
#include <memory>

namespace Pistachio {
	extern InputHandler* CreateDefaultInputHandler();
	Application* Application::s_Instance = nullptr;
	Application::Application(const char* name, const ApplicationOptions& opt)
	{
		PT_PROFILE_FUNCTION();
		s_Instance = this;
		m_headless = opt.headless;
		Pistachio::Log::Init(opt.log_file_name);
		RendererBase::InitOptions ropt;
		ropt.luid = opt.gpu_luid;
		ropt.exportTexture = opt.exportTextures;
		ropt.custom_device = opt.custom_device;
		ropt.custom_instance = opt.custom_instance;
		ropt.custom_physical_device = opt.custom_physical_device;
		ropt.custom_direct_queue = opt.custom_direct_queue;
		ropt.custom_compute_queue = opt.custom_compute_queue;
		ropt.forceSingleQueue = opt.forceSingleQueue;
		ropt.indices = opt.indices;
		ropt.custom_fn = std::move(opt.select_physical_device);
		shaderDir = opt.shader_dir;
		if(!opt.headless) Window_PreGraphicsInit();
		m_rendererBase = std::make_unique<RendererBase>();
		m_rendererBase->Init(ropt);
		if (!opt.headless)
		{
			WindowInfo info;
			info.height = 720;
			info.width = 1280;
			info.vsync = 1;
			info.title = name;
			m_Window = Scope<Window>(Window::Create(info));
			m_Window->SetEventCallback(BIND_EVENT_FN(Application::OnEvent));
			handler = std::unique_ptr<InputHandler>(CreateDefaultInputHandler());
		}

		m_assetManager = std::make_unique<AssetManager>();
		m_renderer = std::make_unique<Renderer>();
		m_renderer->Init();


		Renderer2D::Init();
		//Physics::Init();
#ifdef IMGUI
		PushOverlay(m_ImGuiLayer);
#endif
		InitTime = std::chrono::high_resolution_clock::now();
		lastFrameTime = std::chrono::milliseconds(0);
	}

	Application::~Application()
	{
		RendererBase::FlushGPU();
	}

	void Application::PushLayer(Layer* layer)
	{
		m_layerstack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* overlay)
	{
		m_layerstack.PushOverlay(overlay);
		overlay->OnAttach();
	}
	void Application::SetInputHandler(std::unique_ptr<InputHandler> _handler)
	{
		handler = std::move(_handler);
	}
	void Application::OnEvent(Event& e)
	{
		
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(Application::OnWindowResize));
		Renderer::OnEvent(e);
		for (auto it = m_layerstack.end(); it != m_layerstack.begin(); )
		{
			(*--it)->OnEvent(e);
			if (e.Handled)
				break;
		}
	}
	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		PT_PROFILE_FUNCTION();
		if (e.GetWidth() == 0)
			m_minimized = true;
		else
		{
			//TODO window resize
			m_minimized = false;
		}
		return false;
	}
	Application& Application::Get()
	{
		return *s_Instance; 
	}
	void Application::SetImGuiContext(void* ctx)
	{
		ImGui::SetCurrentContext((ImGuiContext*)ctx);
	}
	void Application::Run()
	{
		while (m_Running) {
			auto now = std::chrono::high_resolution_clock::now();
			std::chrono::milliseconds time = std::chrono::duration_cast<std::chrono::milliseconds>(now - InitTime);
			std::chrono::duration<float> delta;
			delta = time - lastFrameTime;
			lastFrameTime = time;
			FrameMark;
			
			if (!m_Running)
				break;


			if (!m_minimized) {
				for (Layer* layer : m_layerstack)
					layer->OnUpdate(delta.count());
			}
			for (Layer* layer : m_layerstack)
				layer->OnImGuiRender();
			Renderer::EndScene();
			m_Window->OnUpdate(delta.count());
		}
		
	}
	bool Application::Exists()
	{
		return s_Instance;
	}
	void Application::Step()
	{
		auto now = std::chrono::high_resolution_clock::now();
		std::chrono::milliseconds time = std::chrono::duration_cast<std::chrono::milliseconds>(now - InitTime);
		std::chrono::duration<float> delta;
		delta = time - lastFrameTime;
		lastFrameTime = time;
		FrameMark;
		if(!m_headless) m_Window->OnUpdate(delta.count());
		for (Layer* layer : m_layerstack)
			layer->OnUpdate(delta.count());
		for (Layer* layer : m_layerstack)
			layer->OnImGuiRender();
		Renderer::EndScene();
		return;
	}
	
}