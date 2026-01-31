#pragma once

#include "core/common/resource_id.hpp"
#include <array>
#include <vector>

namespace deepbound {

constexpr int CHUNK_SIZE = 32;

class Chunk {
public:
  Chunk(int x, int y);

  auto get_x() const -> int { return m_x; }
  auto get_y() const -> int { return m_y; }

  auto set_tile(int local_x, int local_y, const resource_id_t &tile_id) -> void;
  auto get_tile(int local_x, int local_y) const -> const resource_id_t &;

  auto get_width() const -> int { return CHUNK_SIZE; }
  auto get_height() const -> int { return CHUNK_SIZE; }

private:
  int m_x;
  int m_y;
  std::array<std::array<resource_id_t, CHUNK_SIZE>, CHUNK_SIZE> m_tiles;
};

} // namespace deepbound
