#include "core/worldgen/world_generator.hpp"
#include "core/worldgen/world.hpp"
#include "core/content/tile.hpp"
#include "core/assets/asset_manager.hpp"
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace deepbound
{
world_generator_t::world_generator_t(world_t *world) : world(world), global_width(80000), global_height(1024), sea_level(500)
{
  // Try to cache tiles
  auto &tile_registry = deepbound::tile_registry_t::get();
  stone_tile = tile_registry.get_tile(resource_id_t("deepbound", "rock-granite"));
  dirt_tile = tile_registry.get_tile(resource_id_t("deepbound", "soil-medium-none"));
  grass_tile = tile_registry.get_tile(resource_id_t("deepbound", "soil-medium-normal"));
  water_tile = tile_registry.get_tile(resource_id_t("deepbound", "water"));
  air_tile = nullptr;

  if (!stone_tile)
    std::cerr << "Warning: Stone tile (rock-granite) not found!" << std::endl;
  if (!dirt_tile)
    std::cerr << "Warning: Dirt tile (soil-medium-none) not found!" << std::endl;
  if (!grass_tile)
    std::cerr << "Warning: Grass tile (soil-medium-normal) not found!" << std::endl;
  if (!water_tile)
    std::cerr << "Warning: Water tile (water) not found!" << std::endl;
}

world_generator_t::~world_generator_t()
{
}

void world_generator_t::load_config(const std::string &path)
{
  std::ifstream file(path);
  if (!file.is_open())
  {
    std::cerr << "Failed to open world gen config: " << path << std::endl;
    return;
  }

  nlohmann::json j;
  file >> j;

  // Global
  if (j.contains("global"))
  {
    auto &g = j["global"];
    global_width = g.value("width", 80000);
    global_height = g.value("height", 1024);
    sea_level = g.value("sea_level", 500);
  }

  // Continental Noise Setup
  if (j.contains("continental_noise"))
  {
    auto &cn = j["continental_noise"];
    int seed = cn.value("seed", 1337);
    float frequency = cn.value("frequency", 0.0005f);
    int octaves = cn.value("octaves", 4);
    float lacunarity = cn.value("lacunarity", 2.0f);
    float gain = cn.value("gain", 0.5f);

    // Create FastNoise2 Node
    // Since FastNoise2 is node based, let's use the simplest generator approach first
    // "FractalFBm" is standard Perlin-like layered noise.

    auto signal = FastNoise::New<FastNoise::Simplex>();
    auto fractal = FastNoise::New<FastNoise::FractalFBm>();
    fractal->SetSource(signal);
    fractal->SetOctaveCount(octaves);
    fractal->SetGain(gain);
    fractal->SetLacunarity(lacunarity);

    // Frequency scaling needs to be applied to coordinates or via a DomainScale node
    auto scale = FastNoise::New<FastNoise::DomainScale>();
    scale->SetSource(fractal);
    scale->SetScale(frequency);

    continental_noise = scale;
  }

  // Landforms
  if (j.contains("landforms"))
  {
    for (const auto &l : j["landforms"])
    {
      Landform lf;
      lf.name = l.value("name", "Unknown");
      lf.threshold = l.value("threshold", 0.0f);
      lf.base_height = l.value("base_height", 500.0f);
      lf.height_variance = l.value("height_variance", 50.0f);

      if (l.contains("noise"))
      {
        auto &n = l["noise"];
        lf.noise.frequency = n.value("frequency", 0.01f);
        lf.noise.octaves = n.value("octaves", 3);
        lf.noise.lacunarity = n.value("lacunarity", 2.0f);
        lf.noise.gain = n.value("gain", 0.5f);
        lf.noise.amplitude = n.value("amplitude", 1.0f);
        lf.noise.seed = 1337 + landforms.size(); // Different seeds or same depending on desired correlation

        // Build noise generator for this landform
        auto signal = FastNoise::New<FastNoise::Simplex>();
        auto fractal = FastNoise::New<FastNoise::FractalFBm>();
        fractal->SetSource(signal);
        fractal->SetOctaveCount(lf.noise.octaves);
        fractal->SetGain(lf.noise.gain);
        fractal->SetLacunarity(lf.noise.lacunarity);

        auto scale = FastNoise::New<FastNoise::DomainScale>();
        scale->SetSource(fractal);
        scale->SetScale(lf.noise.frequency);

        landform_noises.push_back(scale);
      }
      else
      {
        // Fallback or empty noise
        landform_noises.push_back(nullptr);
      }

      landforms.push_back(lf);
    }
  }

  std::cout << "World Generator Config Loaded. " << landforms.size() << " landforms." << std::endl;
}

void world_generator_t::generate_chunk(chunk_t *chunk, int chunk_x, int chunk_y)
{
  if (landforms.empty())
    return;

  // Iterate over local chunk coordinates
  for (int x = 0; x < chunk_t::SIZE; x++)
  {
    int global_x = chunk_x * chunk_t::SIZE + x;

    // Calculate surface height for this column
    float surface_height = get_height_at(global_x);

    for (int y = 0; y < chunk_t::SIZE; y++)
    {
      int global_y = chunk_y * chunk_t::SIZE + y;

      const tile_definition_t *tile = air_tile;

      if (global_y < surface_height)
      {
        // Below ground
        if (global_y < surface_height - 5)
        {
          tile = stone_tile;
        }
        else
        {
          tile = dirt_tile;
        }
      }
      else if (global_y == (int)surface_height)
      {
        // Surface layer
        if (global_y < sea_level)
          tile = dirt_tile;
        else
          tile = grass_tile;
      }
      else
      {
        // Above ground
        if (global_y < sea_level)
        {
          tile = water_tile;
        }
      }

      chunk->set_tile(x, y, tile);
    }
  }
}

float world_generator_t::get_height_at(int x)
{
  if (landforms.empty())
    return (float)sea_level;

  float cont_val = 0.0f;
  if (continental_noise)
    cont_val = continental_noise->GenSingle2D(x, 0, 1337);

  // Find the two landforms to blend between
  size_t i1 = 0;
  size_t i2 = 0;
  float t = 0.0f;

  if (cont_val <= landforms[0].threshold)
  {
    i1 = i2 = 0;
    t = 0.0f;
  }
  else if (cont_val >= landforms.back().threshold)
  {
    i1 = i2 = landforms.size() - 1;
    t = 0.0f;
  }
  else
  {
    for (size_t i = 0; i < landforms.size() - 1; i++)
    {
      if (cont_val >= landforms[i].threshold && cont_val < landforms[i + 1].threshold)
      {
        i1 = i;
        i2 = i + 1;
        float range = landforms[i2].threshold - landforms[i1].threshold;
        if (range > 0.0001f)
          t = (cont_val - landforms[i1].threshold) / range;
        else
          t = 0.0f;
        break;
      }
    }
  }

  auto get_lf_height = [&](size_t idx)
  {
    const auto &lf = landforms[idx];
    float noise_val = 0.0f;
    if (idx < landform_noises.size() && landform_noises[idx])
      noise_val = landform_noises[idx]->GenSingle2D(x, 0, 1337 + idx);
    return lf.base_height + (noise_val * lf.height_variance);
  };

  if (i1 == i2)
  {
    return get_lf_height(i1);
  }

  float h1 = get_lf_height(i1);
  float h2 = get_lf_height(i2);

  // Linear interpolation for smooth transition
  return h1 + (h2 - h1) * t;
}

} // namespace deepbound
