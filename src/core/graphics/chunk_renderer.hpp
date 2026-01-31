#pragma once

#include "core/graphics/camera.hpp"
#include "core/graphics/shader.hpp"
#include "core/worldgen/chunk.hpp"
#include <vector>

namespace deepbound
{

class ChunkRenderer
{
public:
  ChunkRenderer();
  ~ChunkRenderer();

  auto render(const Chunk &chunk, const Camera2D &camera, float aspect_ratio = 1.0f) -> void;

private:
  unsigned int m_vao;
  unsigned int m_vbo;
  unsigned int m_ebo; // If using indices

  std::unique_ptr<Shader> m_shader;
};

} // namespace deepbound
