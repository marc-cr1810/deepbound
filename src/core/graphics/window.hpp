#pragma once

#include <string>
#include <functional>

// Forward declaration to avoid including GLFW in the header
struct GLFWwindow;

namespace deepbound
{

class window_t
{
public:
  struct properties_t
  {
    std::string title;
    int width;
    int height;
    bool vsync;
    std::function<void(double, double)> scroll_callback;
  };

  window_t(const properties_t &props);
  ~window_t();

  auto should_close() const -> bool;
  auto update() -> void;
  auto swap_buffers() -> void;

  auto get_native_window() const -> GLFWwindow *
  {
    return m_window;
  }
  auto get_width() const -> int
  {
    return m_data.width;
  }
  auto get_height() const -> int
  {
    return m_data.height;
  }

  // Input
  auto is_key_pressed(int key_code) const -> bool;

  // Callbacks
  using ScrollCallback = std::function<void(double xoffset, double yoffset)>;
  auto set_scroll_callback(const ScrollCallback &callback) -> void
  {
    m_data.scroll_callback = callback;
  }

private:
  auto init(const properties_t &props) -> void;
  auto shutdown() -> void;

  GLFWwindow *m_window{nullptr};
  properties_t m_data;
};

} // namespace deepbound
