#pragma once

#include <vector>
#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include <FastNoise/FastNoise.h>

namespace deepbound
{
// Forward delcaration
class world_t;
struct chunk_t;
struct tile_definition_t;

// Configuration structures
struct NoiseConfig
{
  float frequency;
  int octaves;
  float lacunarity;
  float gain;
  float amplitude;
  int seed;
};

struct Landform
{
  std::string name;
  float threshold; // Continental noise threshold to activate
  float base_height;
  float height_variance;
  NoiseConfig noise;
};

class world_generator_t
{
public:
  world_generator_t(world_t *world);
  ~world_generator_t();

  void load_config(const std::string &path);

  // Main generate function
  void generate_chunk(chunk_t *chunk, int chunk_x, int chunk_y);

private:
  world_t *world;

  int global_width;
  int global_height;
  int sea_level;

  // FastNoise2 Generators
  FastNoise::SmartNode<> continental_noise;
  std::vector<FastNoise::SmartNode<>> landform_noises;
  std::vector<Landform> landforms;

  // Pre-calculated or cached noise objects if needed
  // Actually FastNoise2 uses nodes, so we just setup a graph or separate nodes.
  // For simplicity, we'll likely just create separate GenSimplex or Fractal nodes.

  // Helper to get height at x
  float get_height_at(int x);

  // Tiles needed
  const tile_definition_t *stone_tile = nullptr;
  const tile_definition_t *dirt_tile = nullptr;
  const tile_definition_t *grass_tile = nullptr;
  const tile_definition_t *water_tile = nullptr;
  const tile_definition_t *air_tile = nullptr; // nullptr usually means air but let's be safe.
};

} // namespace deepbound
