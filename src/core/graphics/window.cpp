#include "core/graphics/window.hpp"
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <iostream>

namespace deepbound
{

static bool s_glfw_initialized = false;

static void glfw_error_callback(int error, const char *description)
{
  std::cerr << "GLFW Error (" << error << "): " << description << std::endl;
}

Window::Window(const Properties &props)
{
  init(props);
}

Window::~Window()
{
  shutdown();
}

auto Window::init(const Properties &props) -> void
{
  m_data = props;

  if (!s_glfw_initialized)
  {
    int success = glfwInit();
    if (!success)
    {
      std::cerr << "Could not initialize GLFW!" << std::endl;
      return;
    }
    glfwSetErrorCallback(glfw_error_callback);
    s_glfw_initialized = true;
  }

  // Set context version (e.g. 4.6 Core)
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  m_window = glfwCreateWindow(m_data.width, m_data.height, m_data.title.c_str(), nullptr, nullptr);

  if (!m_window)
  {
    std::cerr << "Could not create GLFW window!" << std::endl;
    glfwTerminate();
    return;
  }

  glfwMakeContextCurrent(m_window);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
  {
    std::cerr << "Failed to initialize GLAD" << std::endl;
    return;
  }

  // Basic user pointer setup
  glfwSetWindowUserPointer(m_window, &m_data);

  // Resize callback
  glfwSetFramebufferSizeCallback(m_window,
                                 [](GLFWwindow *window, int width, int height)
                                 {
                                   glViewport(0, 0, width, height);
                                   Window::Properties *props = (Window::Properties *)glfwGetWindowUserPointer(window);
                                   if (props)
                                   {
                                     props->width = width;
                                     props->height = height;
                                   }
                                 });

  // Scroll callback
  glfwSetScrollCallback(m_window,
                        [](GLFWwindow *window, double xoffset, double yoffset)
                        {
                          Window::Properties *props = (Window::Properties *)glfwGetWindowUserPointer(window);
                          if (props && props->scroll_callback)
                          {
                            props->scroll_callback(xoffset, yoffset);
                          }
                        });

  if (m_data.vsync)
    glfwSwapInterval(1);
  else
    glfwSwapInterval(0);
}

auto Window::shutdown() -> void
{
  if (m_window)
  {
    glfwDestroyWindow(m_window);
  }
}

auto Window::should_close() const -> bool
{
  return glfwWindowShouldClose(m_window);
}

auto Window::update() -> void
{
  glfwPollEvents();
}

auto Window::swap_buffers() -> void
{
  glfwSwapBuffers(m_window);
}

auto Window::is_key_pressed(int key_code) const -> bool
{
  auto state = glfwGetKey(m_window, key_code);
  return state == GLFW_PRESS || state == GLFW_REPEAT;
}

} // namespace deepbound
