#include "core/worldgen/chunk.hpp"

namespace deepbound
{

Chunk::Chunk(int x, int y) : m_x(x), m_y(y)
{
  // Initialize with "air" or empty
  resource_id_t air("deepbound", "air");
  for (auto &row : m_tiles)
  {
    row.fill(air);
  }
}

auto Chunk::set_tile(int local_x, int local_y, const resource_id_t &tile_id) -> void
{
  if (local_x >= 0 && local_x < CHUNK_SIZE && local_y >= 0 && local_y < CHUNK_SIZE)
  {
    m_tiles[local_y][local_x] = tile_id;
    m_mesh_dirty = true;
  }
}

auto Chunk::get_tile(int local_x, int local_y) const -> const resource_id_t &
{
  if (local_x >= 0 && local_x < CHUNK_SIZE && local_y >= 0 && local_y < CHUNK_SIZE)
  {
    return m_tiles[local_y][local_x];
  }
  static resource_id_t air("deepbound", "air");
  return air;
}

} // namespace deepbound
