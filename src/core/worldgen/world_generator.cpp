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

    auto signal = FastNoise::New<FastNoise::Simplex>();
    auto fractal = FastNoise::New<FastNoise::FractalFBm>();
    fractal->SetSource(signal);
    fractal->SetOctaveCount(octaves);
    fractal->SetGain(gain);
    fractal->SetLacunarity(lacunarity);

    auto scale = FastNoise::New<FastNoise::DomainScale>();
    scale->SetSource(fractal);
    scale->SetScale(frequency);

    continental_noise = scale;
  }

  // Climate Noise Setup (Temp)
  {
    auto signal = FastNoise::New<FastNoise::Simplex>();
    auto fractal = FastNoise::New<FastNoise::FractalFBm>();
    fractal->SetSource(signal);
    fractal->SetOctaveCount(3);
    fractal->SetGain(0.5f);
    fractal->SetLacunarity(2.0f);

    auto scale = FastNoise::New<FastNoise::DomainScale>();
    scale->SetSource(fractal);
    scale->SetScale(0.0001f); // Very slow variation for climate

    temp_noise = scale;
  }

  // Climate Noise Setup (Rain)
  {
    auto signal = FastNoise::New<FastNoise::Simplex>();
    auto fractal = FastNoise::New<FastNoise::FractalFBm>();
    fractal->SetSource(signal);
    fractal->SetOctaveCount(3);
    fractal->SetGain(0.5f);
    fractal->SetLacunarity(2.0f);

    auto scale = FastNoise::New<FastNoise::DomainScale>();
    scale->SetSource(fractal);
    scale->SetScale(0.00012f); // Slightly different scale

    rain_noise = scale;
  }

  // Initialize thickness noise (Higher frequency for localized variation, but smooth)
  thickness_noise = FastNoise::New<FastNoise::Perlin>();

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
        lf.noise.seed = 1337 + (int)landforms.size();

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
        landform_noises.push_back(nullptr);
      }

      landforms.push_back(lf);
    }
  }

  std::cout << "World Generator Config Loaded. " << landforms.size() << " landforms." << std::endl;
}

void world_generator_t::load_block_layers(const std::string &path)
{
  std::ifstream file(path);
  if (!file.is_open())
  {
    std::cerr << "Failed to open block layers config: " << path << std::endl;
    return;
  }

  nlohmann::json j;
  file >> j;

  if (j.contains("block_layers"))
  {
    block_layers.clear();
    for (const auto &bl : j["block_layers"])
    {
      BlockLayer layer;
      layer.name = bl.value("name", "Unknown");
      layer.min_temp = bl.value("min_temp", -50.0f);
      layer.max_temp = bl.value("max_temp", 50.0f);
      layer.min_rain = bl.value("min_rain", 0.0f);
      layer.max_rain = bl.value("max_rain", 255.0f);

      if (bl.contains("entries"))
      {
        for (const auto &entry_j : bl["entries"])
        {
          BlockLayerEntry entry;
          entry.tile_code = entry_j.value("tile", "air");

          if (entry_j.contains("thickness"))
          {
            if (entry_j["thickness"].is_array())
            {
              entry.min_thickness = entry_j["thickness"][0];
              entry.max_thickness = entry_j["thickness"][1];
            }
            else
            {
              entry.min_thickness = entry.max_thickness = entry_j.value("thickness", 1);
            }
          }
          else
          {
            entry.min_thickness = entry.max_thickness = 1;
          }

          layer.entries.push_back(entry);
        }
      }

      if (bl.contains("submerged_entries"))
      {
        for (const auto &entry_j : bl["submerged_entries"])
        {
          BlockLayerEntry entry;
          entry.tile_code = entry_j.value("tile", "air");

          if (entry_j.contains("thickness"))
          {
            if (entry_j["thickness"].is_array())
            {
              entry.min_thickness = entry_j["thickness"][0];
              entry.max_thickness = entry_j["thickness"][1];
            }
            else
            {
              entry.min_thickness = entry.max_thickness = entry_j.value("thickness", 1);
            }
          }
          else
          {
            entry.min_thickness = entry.max_thickness = 1;
          }

          layer.submerged_entries.push_back(entry);
        }
      }

      block_layers.push_back(layer);
    }
  }

  std::cout << "Block Layers Loaded. " << block_layers.size() << " layers." << std::endl;
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
    auto climate = get_climate_at(global_x);

    // Pick topsoil layer based on climate
    const BlockLayer *active_layer = nullptr;
    for (const auto &layer : block_layers)
    {
      if (climate.first >= layer.min_temp && climate.first <= layer.max_temp && climate.second >= layer.min_rain && climate.second <= layer.max_rain)
      {
        active_layer = &layer;
        break;
      }
    }

    // Default layer if none matched (plains-like)
    if (!active_layer && !block_layers.empty())
      active_layer = &block_layers[0];

    for (int y = 0; y < chunk_t::SIZE; y++)
    {
      int global_y = chunk_y * chunk_t::SIZE + y;

      const tile_definition_t *tile = air_tile;

      if (global_y <= (int)surface_height)
      {
        // Ground logic
        if (active_layer)
        {
          int depth = (int)surface_height - global_y;
          int current_depth_limit = 0;
          bool placed = false;

          bool is_submerged = surface_height < sea_level;
          const auto &entries = (is_submerged && !active_layer->submerged_entries.empty()) ? active_layer->submerged_entries : active_layer->entries;

          for (const auto &entry : entries)
          {
            // Calculate thickness for this column using smooth noise
            int thickness = entry.min_thickness;
            if (entry.max_thickness > entry.min_thickness && thickness_noise)
            {
              // Use a different seed/offset per entry to avoid correlated thickness
              float t_noise = thickness_noise->GenSingle2D(global_x * 0.1f, (float)current_depth_limit, 444);
              float t_val = (t_noise + 1.0f) * 0.5f;
              thickness += (int)(t_val * (entry.max_thickness - entry.min_thickness + 0.99f));
            }

            current_depth_limit += thickness;

            if (depth < current_depth_limit)
            {
              tile = deepbound::tile_registry_t::get().get_tile(resource_id_t("deepbound", entry.tile_code));
              placed = true;
              break;
            }
          }

          if (!placed)
            tile = stone_tile;
        }
        else
        {
          // Fallback
          if (global_y < surface_height - 5)
            tile = stone_tile;
          else
            tile = dirt_tile;
        }
      }
      else
      {
        // Above ground
        if (global_y < sea_level)
          tile = water_tile;
      }

      chunk->set_tile(x, y, tile);
      // Store climate for rendering (tinting)
      chunk->set_climate(x, y, climate.first, climate.second);
    }
  }
}

std::pair<float, float> world_generator_t::get_climate_at(int x)
{
  float t = 0.0f;
  float r = 128.0f;

  if (temp_noise)
  {
    float val = temp_noise->GenSingle2D(x, 0, 999); // Seed for temp
    t = val * 30.0f + 10.0f;                        // -20 to 40 approx
  }

  if (rain_noise)
  {
    float val = rain_noise->GenSingle2D(x, 0, 888); // Seed for rain
    r = (val + 1.0f) * 0.5f * 255.0f;               // 0 to 255
  }

  return {t, r};
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
