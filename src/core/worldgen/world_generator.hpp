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
  float overhang_strength = 0.0f; // New: How much 3D noise affects this landform
  NoiseConfig noise;
};

struct BlockLayerEntry
{
  std::string tile_code;
  int min_thickness;
  int max_thickness;
  const tile_definition_t *resolved_tile = nullptr;
};

struct BlockLayer
{
  std::string name;
  float min_temp, max_temp;
  float min_rain, max_rain;
  std::vector<BlockLayerEntry> entries;
  std::vector<BlockLayerEntry> submerged_entries;
};

struct CaveConfig
{
  // Cheese (Large Caverns)
  bool cheese_enabled = false;
  NoiseConfig cheese_noise;
  float cheese_threshold = 0.5f;
  float cheese_fade_depth = 30.0f;

  // Worm (Tunnels)
  bool worm_enabled = false;
  NoiseConfig worm_noise;
  float worm_thickness = 0.08f;
  float worm_thickness_variation = 0.0f;
  float worm_fade_depth = 40.0f;

  int global_min_depth = 20;
};

struct ProvinceLayer
{
  std::string tile_code;
  const tile_definition_t *resolved_tile = nullptr;
  std::string type;        // "blob", "layer", "vein"
  float frequency = 0.05f; // For noise
  float threshold = 0.6f;  // Noise threshold
};

struct GeologicalProvince
{
  std::string name;
  std::string deep_stone_code;
  const tile_definition_t *resolved_tile = nullptr;
  std::vector<ProvinceLayer> layers;
};

class world_generator_t
{
public:
  world_generator_t(world_t *world);
  ~world_generator_t();

  void load_config(const std::string &path);
  void load_block_layers(const std::string &path);
  void load_caves(const std::string &path);
  void load_provinces(const std::string &path);

  // Main generate function
  void generate_chunk(chunk_t *chunk, int chunk_x, int chunk_y);

private:
  world_t *world;

  int global_width;
  int global_height;
  int sea_level;
  int global_seed;

  // FastNoise2 Generators
  FastNoise::SmartNode<> continental_noise;
  FastNoise::SmartNode<> temp_noise;
  FastNoise::SmartNode<> rain_noise;

  FastNoise::SmartNode<> thickness_noise;
  FastNoise::SmartNode<> overhang_noise; // New: 3D noise for overhangs

  FastNoise::SmartNode<> cheese_noise;
  FastNoise::SmartNode<> worm_noise;
  FastNoise::SmartNode<> strata_noise;       // generic noise for varying layer thickness
  FastNoise::SmartNode<> province_noise;     // Noise for province selection
  FastNoise::SmartNode<> province_mix_noise; // Noise for mixing stones within provinces

  std::vector<FastNoise::SmartNode<>> landform_noises;
  std::vector<Landform> landforms;
  std::vector<BlockLayer> block_layers;
  std::vector<GeologicalProvince> provinces;
  CaveConfig cave_config;

  // Helper to get height at x
  float get_height_at(int x);

  // Helper to check density at x,y. Returns > 0 if solid.
  // Updated to include cave carving
  float get_density_final(int x, int y, float surface_height, float overhang_strength);

  // Internal helpers
  float get_base_density(int x, int y, float surface_height, float overhang_strength);
  float get_cave_density_modifier(int x, int y, float surface_height);

  // Helper to sample climate
  auto get_climate_at(int x) -> std::pair<float, float>; // temp (-50..50), rain (0..255)

  // Tiles needed
  const tile_definition_t *stone_tile = nullptr;
  const tile_definition_t *dirt_tile = nullptr;
  const tile_definition_t *grass_tile = nullptr;
  const tile_definition_t *water_tile = nullptr;
  const tile_definition_t *air_tile = nullptr;
};

} // namespace deepbound
