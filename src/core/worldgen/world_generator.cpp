#include "core/worldgen/world_generator.hpp"
#include "core/worldgen/world.hpp"
#include "core/content/tile.hpp"
#include "core/assets/asset_manager.hpp"
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <random>
#include <ctime>

namespace deepbound
{
world_generator_t::world_generator_t(world_t *world) : world(world), global_width(80000), global_height(1024), sea_level(500), global_seed(0)
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
    // Check for global seed if not already set, or override
    if (j["global"].contains("seed"))
    {
      global_seed = j["global"].value("seed", 0);
    }

    if (global_seed <= 0)
    {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> distrib(1, 2000000000);
      global_seed = distrib(gen);
      std::cout << "Generated Random Seed: " << global_seed << std::endl;
    }
    else
    {
      std::cout << "Using Configured Seed: " << global_seed << std::endl;
    }

    int seed = global_seed; // Use global seed
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

  // Initialize overhang noise (3D Simplex for structures)
  {
    auto signal = FastNoise::New<FastNoise::Simplex>();
    // We want distinct structures, so standard fractal
    auto fractal = FastNoise::New<FastNoise::FractalFBm>();
    fractal->SetSource(signal);
    fractal->SetOctaveCount(3);
    fractal->SetGain(0.5f);
    fractal->SetLacunarity(2.0f);

    auto scale = FastNoise::New<FastNoise::DomainScale>();
    scale->SetSource(fractal);
    scale->SetScale(0.02f); // Medium frequency for overhangs/caves

    overhang_noise = scale;
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
      lf.overhang_strength = l.value("overhang_strength", 0.0f); // Load new param

      if (l.contains("noise"))
      {
        auto &n = l["noise"];
        lf.noise.frequency = n.value("frequency", 0.01f);
        lf.noise.octaves = n.value("octaves", 3);
        lf.noise.lacunarity = n.value("lacunarity", 2.0f);
        lf.noise.gain = n.value("gain", 0.5f);
        lf.noise.amplitude = n.value("amplitude", 1.0f);
        lf.noise.seed = global_seed + (int)landforms.size() + 100; // Offset seed for landforms

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

  // We need to know density of neighbors to determine surface logic,
  // but for chunk generation simplicity, we'll just check "above" within the chunk
  // and assume connections across chunks work "okay" or are fixed in a second pass.
  // Ideally, we'd query world, but world generation happens before insertion.
  // For now: strictly column-based logic within the chunk for surface detection won't work well for overhangs.
  // Better approach: Calculate density for all blocks in chunk.

  // Iterate over local chunk coordinates
  for (int x = 0; x < chunk_t::SIZE; x++)
  {
    int global_x = chunk_x * chunk_t::SIZE + x;

    // Calculate surface height and climate for this column (used for biome selection)
    float surface_height = get_height_at(global_x); // The "ideal" surface height from heightmap
    auto climate = get_climate_at(global_x);

    // Get overhang strength for this column
    // We need to interpolate it just like height
    // (Copy-paste logic from get_height_at essentially, but retrieving strength)
    // TODO: Refactor landform blending to return a struct of properties.
    // For now, let's just re-calculate or approximate.
    // Optimization: Just get it from the "dominant" landform or blend again.
    // Let's do a quick re-blend for strength:
    float overhang_strength = 0.0f;
    {
      float cont_val = 0.0f;
      if (continental_noise)
        cont_val = continental_noise->GenSingle2D(global_x, 0, global_seed);
      // Find blend
      size_t i1 = 0, i2 = 0;
      float t = 0.0f;
      // ... (Same binary search logic) ...
      // For brevity, assume linear scan matches get_height_at logic
      if (cont_val <= landforms[0].threshold)
      {
        i1 = i2 = 0;
        t = 0;
      }
      else if (cont_val >= landforms.back().threshold)
      {
        i1 = i2 = landforms.size() - 1;
        t = 0;
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
            t = (range > 0.0001f) ? (cont_val - landforms[i1].threshold) / range : 0;
            break;
          }
        }
      }
      overhang_strength = landforms[i1].overhang_strength * (1.0f - t) + landforms[i2].overhang_strength * t;
    }

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
    if (!active_layer && !block_layers.empty())
      active_layer = &block_layers[0];

    for (int y = 0; y < chunk_t::SIZE; y++)
    {
      int global_y = chunk_y * chunk_t::SIZE + y;
      const tile_definition_t *tile = air_tile;

      // 1. Check Density
      float density = get_density(global_x, global_y, surface_height, overhang_strength);

      if (density > 0.0f)
      {
        // It's solid ground
        // 2. Surface Detection
        // A block is "surface" if the block ABOVE it has density <= 0.
        // This allows for overhangs: you can have ground, air, ground, air.
        // We need density at y+1.
        float density_above = get_density(global_x, global_y + 1, surface_height, overhang_strength);
        bool is_surface = (density_above <= 0.0f);

        if (is_surface)
        {
          // Apply Topsoil
          // Problem: existing topsoil logic relied on "depth from surface".
          // With 3D terrain, "depth" is harder.
          // Simple version: If surface, use top layer tile.
          // Better version: Trace limited depth.
          // For now, let's just make the top block the first entry, and maybe 1-2 blocks below it?
          // Actually, simplest "Terraria-like" logic:
          // If Exposed -> Grass.
          // If 1-3 blocks covered -> Dirt.
          // Else -> Stone.
          // We can use a recursive check or just local noise for "how deep is dirt here".
          // Let's implement a "Dirt Depth" noise or constant (e.g., 3-5 blocks).
          // Since we are iterating Y, we don't know "how far from surface" we are if we are deep.
          // BUT, we can just say: If near the "Ideal Heightmap Surface" OR if is_surface...
          // Let's stick to: Is Surface -> Top Entry.
          // Else -> Dirt or Stone?
          // Use density gradient?
          // Let's assume everything is Stone, unless near surface.

          bool is_submerged = global_y < sea_level;
          const auto &entries = (is_submerged && !active_layer->submerged_entries.empty()) ? active_layer->submerged_entries : active_layer->entries;
          if (!entries.empty())
            tile = deepbound::tile_registry_t::get().get_tile(resource_id_t("deepbound", entries[0].tile_code));
          else
            tile = dirt_tile;
        }
        else
        {
          // Underground (Solid above us)
          // Is it Dirt or Stone?
          // In Terraria, dirt goes down quite a bit.
          // Let's us a simple "Depth from ideal surface" check for now, plus some noise.
          // If we are significantly below surface_height, it's stone.
          if (global_y < surface_height - 15) // deeply buried
          {
            tile = stone_tile;
          }
          else
          {
            // Transition zone
            tile = dirt_tile;
          }
        }
      }
      else
      {
        // Air (or Water)
        if (global_y < sea_level)
          tile = water_tile;
      }

      chunk->set_tile(x, y, tile);
      chunk->set_climate(x, y, climate.first, climate.second);
    }
  }
}

float world_generator_t::get_density(int x, int y, float surface_height, float overhang_strength)
{
  // Base density: Positive below surface, negative above.
  float density = surface_height - (float)y; // Simple linear falloff

  // If overhang strength is 0, we behave exactly like heightmap (density = distance to surface)
  // If we want overhangs, we add 3D noise.

  if (overhang_strength > 0.001f && overhang_noise)
  {
    // internal: magnitude_check is pseudo-code, just use 'y'.
    // 3D noise sample.
    // We use x, y and a seed.
    // Note: GenSingle2D is for 2D. We need 3D or just 2D with Y included.
    // Common trick for 2D side scrollers: Use 2D noise where Y is the 2nd dimension!
    // So GenSingle2D(x, y). Correct.
    float noise = overhang_noise->GenSingle2D(x * 1.0f, y * 1.5f, global_seed + 12345);

    // Modulate strength by depth?
    // Usually we want more noise near the surface for overhangs, but less deep down (or maybe caves deep down).
    // For "Overhangs", we effectively want to distort the surface.
    // Equation: surface_height + noise * strength > y
    // <=> surface_height - y + noise * strength > 0
    density = (surface_height - (float)y) + (noise * overhang_strength * 40.0f); // 40.0 arbitrary scale factor for noise amp
  }
  else
  {
    density = surface_height - (float)y;
  }

  return density;
}

std::pair<float, float> world_generator_t::get_climate_at(int x)
{
  float t = 0.0f;
  float r = 128.0f;

  if (temp_noise)
  {
    float val = temp_noise->GenSingle2D(x, 0, global_seed + 999); // Seed for temp
    t = val * 30.0f + 10.0f;                                      // -20 to 40 approx
  }

  if (rain_noise)
  {
    float val = rain_noise->GenSingle2D(x, 0, global_seed + 888); // Seed for rain
    r = (val + 1.0f) * 0.5f * 255.0f;                             // 0 to 255
  }

  return {t, r};
}

float world_generator_t::get_height_at(int x)
{
  if (landforms.empty())
    return (float)sea_level;

  float cont_val = 0.0f;
  if (continental_noise)
    cont_val = continental_noise->GenSingle2D(x, 0, global_seed);

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
      noise_val = landform_noises[idx]->GenSingle2D(x, 0, global_seed + 1337 + idx);
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
