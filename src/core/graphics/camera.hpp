#pragma once

#include <glm/glm.hpp>
#include <algorithm>

namespace deepbound
{

class Camera2D
{
public:
  Camera2D() = default;

  auto set_position(const glm::vec2 &pos) -> void
  {
    m_position = pos;
  }
  auto get_position() const -> const glm::vec2 &
  {
    return m_position;
  }

  auto move(const glm::vec2 &delta) -> void
  {
    m_position += delta;
  }

  auto set_zoom(float zoom) -> void
  {
    m_zoom = std::clamp(zoom, m_min_zoom, m_max_zoom);
  }
  auto get_zoom() const -> float
  {
    return m_zoom;
  }

  auto zoom_by(float factor) -> void
  {
    set_zoom(m_zoom * factor);
  }

  // Zoom in/out by adding/subtracting for intuitive scroll behavior
  auto zoom_scroll(float offset) -> void
  {
    float speed = 0.1f;
    float new_zoom = m_zoom;
    if (offset > 0)
      new_zoom *= (1.0f + speed);
    else if (offset < 0)
      new_zoom /= (1.0f + speed);
    set_zoom(new_zoom);
  }

private:
  glm::vec2 m_position{0.0f, 0.0f};
  float m_zoom{1.0f};
  float m_min_zoom{0.1f};
  float m_max_zoom{10.0f};
};

} // namespace deepbound
