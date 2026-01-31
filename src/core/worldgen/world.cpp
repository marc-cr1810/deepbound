#include "core/worldgen/world.hpp"
#include <iostream>

namespace deepbound
{

World::World()
{
}

auto World::get_chunk(int chunk_x, int chunk_y) -> Chunk *
{
  std::pair<int, int> pos = {chunk_x, chunk_y};
  if (m_chunks.find(pos) == m_chunks.end())
  {
    // Generate on demand
    // std::cout << "Generating Chunk: " << chunk_x << ", " << chunk_y << std::endl;
    m_chunks[pos] = m_generator.generate_chunk(chunk_x, chunk_y);
  }
  return m_chunks[pos].get();
}

auto World::update(const glm::vec2 &camera_pos) -> void
{
  // In a real game, unloading logic would go here.
  // For now, we just grow infinitely.
}

auto World::get_visible_chunks(const glm::vec2 &camera_pos, int range) -> std::vector<Chunk *>
{
  std::vector<Chunk *> visible;

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
      visible.push_back(get_chunk(center_chunk_x + x, center_chunk_y + y));
    }
  }
  return visible;
}

} // namespace deepbound
