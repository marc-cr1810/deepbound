#pragma once

#include "core/worldgen/chunk.hpp"
#include "core/worldgen/world_generator.hpp"
#include <map>
#include <memory>
#include <vector>
#include <optional>
#include <glm/glm.hpp>

namespace deepbound
{

class world_t
{
public:
  world_t();

  auto update(const glm::vec2 &camera_pos) -> void;

  // Get visible chunks for rendering.
  // Range is in "chunks" radius.
  auto get_visible_chunks(const glm::vec2 &camera_pos, int range = 2) -> std::vector<chunk_t *>;

  auto get_tile_at(float world_x, float world_y) -> std::optional<resource_id_t>;

private:
  auto get_chunk(int x, int y) -> chunk_t *;

  std::map<std::pair<int, int>, std::unique_ptr<chunk_t>> m_chunks;
  world_generator_t m_generator;
};

} // namespace deepbound
