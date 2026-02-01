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

  // Climate data [x][y] -> {temp, rain}
  // Stored as normalized bytes 0-255 for compact storage, mapping to -50..50 (temp) and 0..255 (rain) or similar.
  // Actually, let's just store floats for accuracy similar to generation, or bytes if strict on memory.
  // Given 32x32, floats are fine (32*32*2*4 = 8KB per chunk).
  struct climate_t
  {
    float temp;
    float rain;
  };
  std::array<std::array<climate_t, CHUNK_SIZE>, CHUNK_SIZE> m_climate;

public:
  auto set_climate(int local_x, int local_y, float temp, float rain) -> void
  {
    if (local_x >= 0 && local_x < CHUNK_SIZE && local_y >= 0 && local_y < CHUNK_SIZE)
    {
      m_climate[local_x][local_y] = {temp, rain};
    }
  }
  auto get_climate(int local_x, int local_y) const -> climate_t
  {
    if (local_x >= 0 && local_x < CHUNK_SIZE && local_y >= 0 && local_y < CHUNK_SIZE)
    {
      return m_climate[local_x][local_y];
    }
    return {0.0f, 0.0f};
  }
};

} // namespace deepbound
