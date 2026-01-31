#include "core/worldgen/world_generator.hpp"
#include "core/worldgen/landform.hpp"
#include "core/worldgen/rock_strata.hpp"

namespace deepbound
{

WorldGenerator::WorldGenerator()
{
}

auto WorldGenerator::generate_chunk(int chunk_x, int chunk_y) -> std::unique_ptr<Chunk>
{
  auto chunk = std::make_unique<Chunk>(chunk_x, chunk_y);

  // Simple Test Generation: Validates interaction with Strata/Landform systems
  // ideally For now: Basic flat terrain at y=0 (middle of world if we assume
  // vertical chunks?) Let's assume standard intuitive coordinates: y increases
  // upwards.

  // If we are looking at chunk (0,0), let's fill the bottom half.

  resource_id_t stone("deepbound", "rock-granite");
  resource_id_t dirt("deepbound", "soil-medium");
  resource_id_t air("deepbound", "air");

  for (int x = 0; x < CHUNK_SIZE; ++x)
  {
    for (int y = 0; y < CHUNK_SIZE; ++y)
    {
      // Global Y coordinate
      int global_y = chunk_y * CHUNK_SIZE + y;

      if (global_y < 10)
      {
        chunk->set_tile(x, y, stone);
      }
      else if (global_y < 15)
      {
        chunk->set_tile(x, y, dirt);
      }
      else
      {
        chunk->set_tile(x, y, air);
      }
    }
  }

  return chunk;
}

} // namespace deepbound
