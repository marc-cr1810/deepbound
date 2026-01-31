#pragma once

#include "core/worldgen/chunk.hpp"
#include <memory>

namespace deepbound {

class WorldGenerator {
public:
  WorldGenerator();

  auto generate_chunk(int chunk_x, int chunk_y) -> std::unique_ptr<Chunk>;

private:
  // Future: Seed, Noise generators, etc.
};

} // namespace deepbound
