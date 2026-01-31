#pragma once

#include <memory>
#include "core/graphics/camera.hpp"
#include "core/graphics/shader.hpp"
#include "core/worldgen/chunk.hpp"
#include <vector>

namespace deepbound
{

class chunk_renderer_t
{
public:
  chunk_renderer_t();
  ~chunk_renderer_t();

  auto render(const chunk_t &chunk, const camera_2d_t &camera, float aspect_ratio = 1.0f) -> void;

private:
  unsigned int m_vao;
  unsigned int m_vbo;
  unsigned int m_ebo; // If using indices

  std::unique_ptr<shader_t> m_shader;
};

} // namespace deepbound
