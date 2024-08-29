#pragma once

#include "Ptr.h"
#include "Surface.h"
#include "SwapChain.h"
#include "Texture.h"
#include "ptpch.h"
#include "Pistachio/Core.h"
#include "Pistachio/Event/Event.h"

void SetWindowDataPtr(void* value);
PISTACHIO_API void* GetWindowDataPtr();


	struct PlatformData {
		#ifdef PT_PLATFORM_WINDOWS
		HWND hwnd = NULL;
		#elif defined(PT_PLATFORM_LINUX)
		GLFWwindow* window;

		#endif // PT_PLATFORM_WINDOWS


	};

using EventCallbackFn = std::function<void(Pistachio::Event& e)>;
	struct WindowData
	{
		unsigned int width = 0;
		unsigned int height = 0;
		const char* title;
		unsigned int vsync = 1;
		float dpiscale;
		EventCallbackFn EventCallback;
	};

	namespace Pistachio {

		struct WindowInfo
		{
			unsigned int width;
			unsigned int height;
			const char* title;
			unsigned int vsync;
			WindowInfo(unsigned int w = 1280, unsigned int h = 720, const char* t = "Pistachio Engine", unsigned int Vsync = 1)
				: width(w), height(h), title(t), vsync(Vsync) {}
		};
		class PISTACHIO_API SwapChain
		{
		public:
			RHI::Surface surface;
			RHI::Ptr<RHI::SwapChain> swapchain;
			void Update();
			void Initialize(uint32_t width, uint32_t height);
			std::vector<RHI::Ptr<RHI::Texture>> swapTextures;
		private:
			RHI::Ptr<RHI::DescriptorHeap> mainRTVheap;
			void BackBufferBarrier(RHI::PipelineStage,RHI::PipelineStage,
				RHI::ResourceLayout,RHI::ResourceLayout,
				RHI::ResourceAcessFlags,RHI::ResourceAcessFlags);
		};
		extern void Window_PreGraphicsInit();
		class PISTACHIO_API Window
		{
		public:

			virtual ~Window() {}
			virtual void OnUpdate(float) = 0;

			virtual unsigned int GetWidth() const = 0;
			virtual unsigned int GetHeight() const = 0;

			virtual void SetVsync(unsigned int enabled) = 0;
			virtual void SetEventCallback(const EventCallbackFn& event) = 0;
			virtual unsigned int IsVsync() const = 0;
			static Window* Create(const WindowInfo& info = WindowInfo());
			PlatformData pd;
			SwapChain& GetSwapChain() {return m_swapChain;}
		protected:
			WindowData m_data;
			SwapChain m_swapChain;
		};
		
		
	}

