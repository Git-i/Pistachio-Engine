#include "Core.h"
#include "GLFW/glfw3.h"
#include "Pistachio/Core.h"
#include "Pistachio/Core/Application.h"
#include "LinuxWindow.h"
#include "Pistachio/Core/Log.h"
#include "Pistachio/Core/Window.h"
#include "Pistachio/Renderer/RendererBase.h"
#ifdef IMGUI
#include "imgui.h"
#include "imgui_impl_glfw.h"
#endif
#include "Pistachio/Core/InputCallbacks.h"

void* WindowDataPtr;

void* GetWindowDataPtr()
{
	return WindowDataPtr;
}
void SetWindowDataPtr(void* value)
{
	WindowDataPtr = value;
}
int GLFW_to_PT(int key);
int PT_to_GLFW(int key);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS)
        Pistachio::OnKeyDown(key);
    else if (action == GLFW_RELEASE)
        Pistachio::OnKeyUp(key);
    
}
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    Pistachio::OnMouseMove(xpos, ypos);
}
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    Pistachio::OnMouseScroll(xoffset, yoffset);
}
void window_close(GLFWwindow* window)
{
	Pistachio::Application::Get().Stop();
}
void window_resize(GLFWwindow* window, int w, int h)
{
	PT_CORE_TRACE("Window Resize");
	PT_DEBUG_BREAK;
}


namespace Pistachio {
	void Window_PreGraphicsInit()
	{
		glfwInit();
	}
	Window* Window::Create(const WindowInfo& info)
	{
		return new LinuxWindow(info);
	}
	LinuxWindow::LinuxWindow(const WindowInfo& info)
	{
		Init(info);
	}
	LinuxWindow::~LinuxWindow()
	{
		Shutdown();
	}
	void error_callback(int code, const char* details)
	{
		PT_CORE_ERROR("GLFW Error({0}): {1}", code, details);
		DEBUG_BREAK;
	}
	int LinuxWindow::Init(const WindowInfo& info)
	{
		PT_PROFILE_FUNCTION();
		SetWindowDataPtr(&m_data);
		m_data.title = info.title;
		m_data.height = info.height;
		m_data.width = info.width;
		m_data.vsync = info.vsync;
		glfwSetErrorCallback(error_callback);
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        pd.window = glfwCreateWindow(info.width, info.height, info.title, NULL, NULL);
        glfwSetKeyCallback(pd.window, key_callback);
        glfwSetCursorPosCallback(pd.window, cursor_position_callback);
        glfwSetScrollCallback(pd.window, scroll_callback);
		glfwSetWindowCloseCallback(pd.window, window_close);
		glfwSetWindowSizeCallback(pd.window, window_resize);
		m_swapChain.surface.InitGLFW(pd.window, RendererBase::GetInstance()->ID);
		m_swapChain.Initialize(info.width, info.height);
        return 0;
	}
	void LinuxWindow::Shutdown()
	{
        if(pd.window)
        glfwDestroyWindow(pd.window);
		glfwTerminate();
        pd.window = nullptr;
	}
	void LinuxWindow::OnUpdate(float delta)
	{
		#ifdef _DEBUG
			int a = int(1/(delta));
			std::string title = std::string("FPS: ") + std::to_string(a);
			glfwSetWindowTitle(pd.window, title.c_str());
		#endif // _DEBUG
		m_swapChain.Update();
        glfwPollEvents();
	}
	void LinuxWindow::SetVsync(unsigned int enabled)
	{
		m_data.vsync = enabled;
	}
	unsigned int LinuxWindow::IsVsync() const
	{
		return m_data.vsync;
	}
}

