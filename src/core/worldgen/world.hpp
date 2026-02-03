#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <glm/glm.hpp>

// Forward declarations
namespace deepbound
{
struct tile_definition_t;
class chunk_renderer_t;
} // namespace deepbound

namespace deepbound
{

// Simple chunk structure for now
struct climate_info_t
{
  float temp = 0.0f;
  float rain = 0.0f;
};

// Simple chunk structure for now
struct chunk_t
{
  int x; // Chunk coordinate X (in chunk units? or world units? renderer uses get_x * SIZE so chunk units)
  int y; // Chunk coordinate Y

  static const int SIZE = 32;
  static const int WORLD_HEIGHT = 1024;

  std::vector<const tile_definition_t *> tiles; // Size: SIZE * SIZE
  std::vector<climate_info_t> climate;          // Size: SIZE * SIZE

  // Mesh cache
  std::vector<float> mesh;
  bool mesh_dirty = true;

  chunk_t()
  {
    tiles.resize(SIZE * SIZE, nullptr);
    climate.resize(SIZE * SIZE);
  }

  const tile_definition_t *get_tile(int local_x, int local_y) const
  {
    if (local_x < 0 || local_x >= SIZE || local_y < 0 || local_y >= SIZE)
      return nullptr;
    return tiles[local_x * SIZE + local_y];
  }

  void set_tile(int local_x, int local_y, const tile_definition_t *tile)
  {
    if (local_x < 0 || local_x >= SIZE || local_y < 0 || local_y >= SIZE)
      return;
    tiles[local_x * SIZE + local_y] = tile;
    mesh_dirty = true;
  }

  climate_info_t get_climate(int local_x, int local_y) const
  {
    if (local_x < 0 || local_x >= SIZE || local_y < 0 || local_y >= SIZE)
      return {};
    return climate[local_x * SIZE + local_y];
  }

  void set_climate(int local_x, int local_y, float temp, float rain)
  {
    if (local_x < 0 || local_x >= SIZE || local_y < 0 || local_y >= SIZE)
      return;
    climate[local_x * SIZE + local_y] = {temp, rain};
    mesh_dirty = true;
  }

  bool is_mesh_dirty() const
  {
    return mesh_dirty;
  }
  void set_mesh(std::vector<float> new_mesh)
  {
    mesh = std::move(new_mesh);
    mesh_dirty = false;
  }
  const std::vector<float> &get_mesh() const
  {
    return mesh;
  }

  int get_x() const
  {
    return x / SIZE;
  } // If x is world units? No, let's treat x as world coord for now based on previous code `new_chunk->x = cx * core::SIZE`.
  // Wait, renderer line 210: chunk_world_x = (float)chunk.get_x() * CHUNK_SIZE;
  // This implies `get_x` returns the CHUNK COORDINATE.
  // In `world.cpp`: `new_chunk->x = cx * chunk_t::SIZE`. That's WORLD coordinate.
  // If I return `x / SIZE`, that matches the renderer expectation if x is storage for world coord.
  int get_y() const
  {
    return y / SIZE;
  } // Same.
};

class world_t
{
public:
  world_t();
  ~world_t();

  void update(double delta_time);

  // Core access
  const tile_definition_t *get_tile_at(float world_x, float world_y) const;
  const tile_definition_t *get_tile_at(int x, int y) const;

  // Helper
  bool is_solid(float x, float y) const;

  // Chunk management
  // For now, let's just generate everything around the camera or 0,0
  std::vector<chunk_t *> get_visible_chunks(const glm::vec2 &camera_pos, int view_distance);

  // Get chunk at chunk coords
  chunk_t *get_chunk(int cx, int cy);

private:
  std::unordered_map<long long, std::unique_ptr<chunk_t>> chunks;

  long long get_chunk_key(int cx, int cy) const
  {
    return ((long long)cx << 32) | (unsigned int)cy;
  }

  // The Generator
  std::unique_ptr<class world_generator_t> generator;
};

} // namespace deepbound
