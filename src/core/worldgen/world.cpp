#include "core/worldgen/world.hpp"
#include <iostream>
#include <cmath>

namespace deepbound
{

world_t::world_t()
{
}

auto world_t::get_chunk(int chunk_x, int chunk_y) -> chunk_t *
{
  {
    std::lock_guard<std::mutex> lock(m_chunks_mutex);
    auto it = m_chunks.find({chunk_x, chunk_y});
    if (it != m_chunks.end())
    {
      return it->second.get();
    }

    if (m_generating.count({chunk_x, chunk_y}))
    {
      return nullptr;
    }

    // Mark as generating
    m_generating.insert({chunk_x, chunk_y});
  }

  // Launch async generation
  std::thread(
      [this, chunk_x, chunk_y]()
      {
        auto chunk = m_generator.generate_chunk(chunk_x, chunk_y);

        std::lock_guard<std::mutex> lock(m_chunks_mutex);
        m_chunks[{chunk_x, chunk_y}] = std::move(chunk);
        m_generating.erase({chunk_x, chunk_y});
      })
      .detach();

  return nullptr;
}

auto world_t::get_tile_at(float world_x, float world_y) -> std::optional<resource_id_t>
{
  // floor() is essential because negative casts truncate towards zero, but we want grid coordinates
  int tx = (int)std::floor(world_x);
  int ty = (int)std::floor(world_y);

  int cx = (int)std::floor((float)tx / (float)CHUNK_SIZE);
  int cy = (int)std::floor((float)ty / (float)CHUNK_SIZE);

  chunk_t *chunk = get_chunk(cx, cy);
  if (!chunk)
    return std::nullopt;

  int lx = tx % CHUNK_SIZE;
  int ly = ty % CHUNK_SIZE;

  // Handle wrap-around for negative modulo in C++
  if (lx < 0)
    lx += CHUNK_SIZE;
  if (ly < 0)
    ly += CHUNK_SIZE;

  return chunk->get_tile(lx, ly);
}

auto world_t::update(const glm::vec2 &camera_pos) -> void
{
  // In a real game, unloading logic would go here.
  // For now, we just grow infinitely.
}

auto world_t::get_visible_chunks(const glm::vec2 &camera_pos, int range) -> std::vector<chunk_t *>
{
  std::vector<chunk_t *> visible;

  // camera_pos is in world units (blocks).
  // Convert to chunk coordinates.
  // Assuming blocks are 1x1 units for now.
  // CHUNK_SIZE is 32.

  int center_chunk_x = (int)std::floor(camera_pos.x / CHUNK_SIZE);
  int center_chunk_y = (int)std::floor(camera_pos.y / CHUNK_SIZE);

  // Simple square radius
  for (int y = -range; y <= range; ++y)
  {
    for (int x = -range; x <= range; ++x)
    {
      chunk_t *chunk = get_chunk(center_chunk_x + x, center_chunk_y + y);
      if (chunk)
      {
        visible.push_back(chunk);
      }
    }
  }
  return visible;
}

} // namespace deepbound
