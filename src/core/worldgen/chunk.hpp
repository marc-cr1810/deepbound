#pragma once

#include "core/common/resource_id.hpp"
#include <array>
#include <vector>
#include <utility>

namespace deepbound
{

constexpr int CHUNK_SIZE = 32;

class chunk_t
{
public:
  chunk_t(int x, int y);

  auto get_x() const -> int
  {
    return m_x;
  }
  auto get_y() const -> int
  {
    return m_y;
  }

  auto set_tile(int local_x, int local_y, const resource_id_t &tile_id) -> void;
  auto get_tile(int local_x, int local_y) const -> const resource_id_t &;

  auto get_width() const -> int
  {
    return CHUNK_SIZE;
  }
  auto get_height() const -> int
  {
    return CHUNK_SIZE;
  }

  // Mesh Caching
  auto set_mesh(std::vector<float> &&vertices) const -> void
  {
    m_mesh_vertices = std::move(vertices);
    m_mesh_dirty = false;
  }
  auto get_mesh() const -> const std::vector<float> &
  {
    return m_mesh_vertices;
  }
  auto is_mesh_dirty() const -> bool
  {
    return m_mesh_dirty;
  }
  auto mark_mesh_dirty() const -> void
  {
    m_mesh_dirty = true;
  }

private:
  int m_x;
  int m_y;
  std::array<std::array<resource_id_t, CHUNK_SIZE>, CHUNK_SIZE> m_tiles;

  mutable std::vector<float> m_mesh_vertices;
  mutable bool m_mesh_dirty = true;
};

} // namespace deepbound
