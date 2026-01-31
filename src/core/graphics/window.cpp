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

window_t::window_t(const properties_t &props)
{
  init(props);
}

window_t::~window_t()
{
  shutdown();
}

auto window_t::init(const properties_t &props) -> void
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
                                   window_t::properties_t *props = (window_t::properties_t *)glfwGetWindowUserPointer(window);
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
                          window_t::properties_t *props = (window_t::properties_t *)glfwGetWindowUserPointer(window);
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

auto window_t::shutdown() -> void
{
  if (m_window)
  {
    glfwDestroyWindow(m_window);
  }
}

auto window_t::should_close() const -> bool
{
  return glfwWindowShouldClose(m_window);
}

auto window_t::update() -> void
{
  glfwPollEvents();
}

auto window_t::swap_buffers() -> void
{
  glfwSwapBuffers(m_window);
}

auto window_t::is_key_pressed(int key_code) const -> bool
{
  auto state = glfwGetKey(m_window, key_code);
  return state == GLFW_PRESS || state == GLFW_REPEAT;
}

} // namespace deepbound
