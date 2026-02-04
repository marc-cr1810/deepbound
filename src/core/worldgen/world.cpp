#include "core/worldgen/world.hpp"
#include "core/worldgen/world_generator.hpp"
#include "core/content/tile.hpp"
#include <iostream>

namespace deepbound
{

world_t::world_t()
{
  // Initialize Generator
  generator = std::make_unique<world_generator_t>(this);
  // Load config relative to executable or known path
  generator->load_config("assets/worldgen/landforms.json");
  generator->load_block_layers("assets/worldgen/blocklayers.json");
  generator->load_caves("assets/worldgen/caves.json");

  // Pre-generate some chunks around user spawn?
  // Let's just generate the origin (0,0) chunk for now to verify.
  // get_chunk(0, 0);
}

world_t::~world_t()
{
}

void world_t::update(double delta_time)
{
  // Update chunks, simulate water, entities...
}

const tile_definition_t *world_t::get_tile_at(float world_x, float world_y) const
{
  int tx = (int)floor(world_x);
  int ty = (int)floor(world_y);
  return get_tile_at(tx, ty);
}

const tile_definition_t *world_t::get_tile_at(int x, int y) const
{
  // Calculate chunk coordinates
  // Chunks are 32x32
  int cx = x / chunk_t::SIZE;
  int cy = y / chunk_t::SIZE;

  // Handle negative coords correctly for integer division towards -inf
  if (x < 0 && x % chunk_t::SIZE != 0)
    cx -= 1;
  if (y < 0 && y % chunk_t::SIZE != 0)
    cy -= 1;

  // Check key
  long long key = ((long long)cx << 32) | (unsigned int)cy;
  auto search = chunks.find(key);
  if (search != chunks.end())
  {
    const auto &chunk = search->second;
    // Local coords
    int lx = x - (cx * chunk_t::SIZE);
    int ly = y - (cy * chunk_t::SIZE);

    // Ensure positive modulo
    // lx should naturally be 0..31 if we did floor math right
    // Let's rely on get_tile bounds check
    return chunk->get_tile(lx, ly);
  }

  return nullptr; // No chunk or Air
}

bool world_t::is_solid(float x, float y) const
{
  auto t = get_tile_at(x, y);
  return t != nullptr; // Non-null means solid for now.
}

std::vector<chunk_t *> world_t::get_visible_chunks(const glm::vec2 &camera_pos, int view_distance)
{
  std::vector<chunk_t *> visible;

  int cx_start = (int)floor(camera_pos.x) / chunk_t::SIZE - view_distance;
  int cx_end = (int)floor(camera_pos.x) / chunk_t::SIZE + view_distance;
  int cy_start = (int)floor(camera_pos.y) / chunk_t::SIZE - view_distance;
  int cy_end = (int)floor(camera_pos.y) / chunk_t::SIZE + view_distance;

  // Ensure height bounds too? Max 1024 -> 32 chunks high
  // Chunks start at Y=0 usually?
  if (cy_start < 0)
    cy_start = 0;
  if (cy_end > 32)
    cy_end = 32;

  for (int cx = cx_start; cx <= cx_end; cx++)
  {
    for (int cy = cy_start; cy <= cy_end; cy++)
    {
      visible.push_back(get_chunk(cx, cy));
    }
  }
  return visible;
}

chunk_t *world_t::get_chunk(int cx, int cy)
{
  long long key = get_chunk_key(cx, cy);

  // Check if exists
  if (chunks.find(key) == chunks.end())
  {
    // Generate
    auto new_chunk = std::make_unique<chunk_t>();
    new_chunk->x = cx * chunk_t::SIZE; // World X origin
    new_chunk->y = cy * chunk_t::SIZE; // World Y origin

    if (generator)
    {
      generator->generate_chunk(new_chunk.get(), cx, cy);
    }

    chunks[key] = std::move(new_chunk);
  }

  return chunks[key].get();
}

} // namespace deepbound
