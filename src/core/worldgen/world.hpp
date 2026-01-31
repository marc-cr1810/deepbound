#pragma once

#include "core/worldgen/chunk.hpp"
#include "core/worldgen/world_generator.hpp"
#include <map>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace deepbound
{

class World
{
public:
  World();

  auto update(const glm::vec2 &camera_pos) -> void;

  // Get visible chunks for rendering.
  // Range is in "chunks" radius.
  auto get_visible_chunks(const glm::vec2 &camera_pos, int range = 2) -> std::vector<Chunk *>;

private:
  auto get_chunk(int x, int y) -> Chunk *;

  std::map<std::pair<int, int>, std::unique_ptr<Chunk>> m_chunks;
  WorldGenerator m_generator;
};

} // namespace deepbound
