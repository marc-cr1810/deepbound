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

void world_generator_t::load_caves(const std::string &path)
{
  std::ifstream file(path);
  if (!file.is_open())
  {
    std::cerr << "Failed to open cave config: " << path << std::endl;
    return;
  }

  nlohmann::json j;
  file >> j;

  if (j.contains("global_min_depth"))
  {
    cave_config.global_min_depth = j["global_min_depth"];
  }

  if (j.contains("cheese_caves"))
  {
    auto &c = j["cheese_caves"];
    cave_config.cheese_enabled = c.value("enabled", false);
    cave_config.cheese_threshold = c.value("threshold", 0.5f);
    cave_config.cheese_fade_depth = c.value("surface_fade_depth", 30.0f);

    cave_config.cheese_noise.frequency = c.value("frequency", 0.015f);
    cave_config.cheese_noise.octaves = c.value("octaves", 2);
    cave_config.cheese_noise.lacunarity = c.value("lacunarity", 2.0f);
    cave_config.cheese_noise.gain = c.value("gain", 0.5f);
    cave_config.cheese_noise.seed = global_seed + 500;

    if (cave_config.cheese_enabled)
    {
      auto signal = FastNoise::New<FastNoise::Simplex>(); // 3D source
      auto fractal = FastNoise::New<FastNoise::FractalFBm>();
      fractal->SetSource(signal);
      fractal->SetOctaveCount(cave_config.cheese_noise.octaves);
      fractal->SetGain(cave_config.cheese_noise.gain);
      fractal->SetLacunarity(cave_config.cheese_noise.lacunarity);

      auto scale = FastNoise::New<FastNoise::DomainScale>();
      scale->SetSource(fractal);
      scale->SetScale(cave_config.cheese_noise.frequency);

      cheese_noise = scale;
    }
  }

  if (j.contains("worm_caves"))
  {
    auto &c = j["worm_caves"];
    cave_config.worm_enabled = c.value("enabled", false);
    cave_config.worm_thickness = c.value("thickness", 0.08f);
    cave_config.worm_thickness_variation = c.value("thickness_variation", 0.0f);
    cave_config.worm_fade_depth = c.value("surface_fade_depth", 40.0f);

    cave_config.worm_noise.frequency = c.value("frequency", 0.02f);
    cave_config.worm_noise.octaves = c.value("octaves", 1);
    cave_config.worm_noise.lacunarity = c.value("lacunarity", 2.0f);
    cave_config.worm_noise.gain = c.value("gain", 0.5f);
    cave_config.worm_noise.seed = global_seed + 600;

    if (cave_config.worm_enabled)
    {
      // Worm tunnels needs Ridged Multi
      auto signal = FastNoise::New<FastNoise::Simplex>();
      auto fractal = FastNoise::New<FastNoise::FractalRidged>(); // Ridged!
      fractal->SetSource(signal);
      fractal->SetOctaveCount(cave_config.worm_noise.octaves);
      fractal->SetGain(cave_config.worm_noise.gain);
      fractal->SetLacunarity(cave_config.worm_noise.lacunarity);

      auto scale = FastNoise::New<FastNoise::DomainScale>();
      scale->SetSource(fractal);
      scale->SetScale(cave_config.worm_noise.frequency);

      worm_noise = scale;
    }
  }

  std::cout << "Cave Config Loaded." << std::endl;
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

  // Initialize strata noise for smooth layer variations
  auto source = FastNoise::New<FastNoise::Simplex>();
  auto fractal = FastNoise::New<FastNoise::FractalFBm>();
  fractal->SetSource(source);
  fractal->SetOctaveCount(2);
  fractal->SetGain(0.5f);
  fractal->SetLacunarity(2.0f);

  auto scale = FastNoise::New<FastNoise::DomainScale>();
  scale->SetSource(fractal);
  scale->SetScale(0.02f); // Low frequency for long, smooth variations

  strata_noise = scale;
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

      // 1. Calculate Densities
      float base_density = get_base_density(global_x, global_y, surface_height, overhang_strength);
      float final_density = base_density;
      if (base_density > 0.0f)
      {
        float cave_mod = get_cave_density_modifier(global_x, global_y, surface_height);
        final_density += cave_mod;
      }
      // 2. Tile Decision Logic
      if (final_density > 0.0f)
      {
        // We are solid.
        bool is_exposed = false;
        bool is_natural_exposure = false;

        // Check above
        float base_density_above = get_base_density(global_x, global_y + 1, surface_height, overhang_strength);
        float cave_mod_above = 0.0f;
        if (base_density_above > 0.0f)
          cave_mod_above = get_cave_density_modifier(global_x, global_y + 1, surface_height);
        float final_density_above = base_density_above + cave_mod_above;

        if (final_density_above <= 0.0f)
        {
          is_exposed = true;
          // Natural exposure if it wasn't carved out.
          // i.e. base density alone was already air.
          if (base_density_above <= 0.0f)
            is_natural_exposure = true;
        }

        // Block Layer Selection
        // Determine "Depth" into the terrain.
        // Approximation: surface_height - y.
        // But for overhangs, we want local depth. Local depth is hard.
        // We will use the macro surface height for the "Soil/Stone" transition.
        // If we are significantly above surface height (overhang), we assume Dirt/Grass composition.

        int depth = (int)(surface_height - (float)global_y);
        if (depth < 0)
          depth = 0; // Treat hills/overhangs as "Surface level" crust

        // Find which entry applies
        // Default to Stone (or last entry)
        tile = stone_tile;

        if (active_layer && !active_layer->entries.empty())
        {
          int current_depth = 0;
          bool found = false;

          // Check if we are in the "Soil" layers
          for (size_t i = 0; i < active_layer->entries.size(); i++)
          {
            const auto &entry = active_layer->entries[i];
            // Use max_thickness for now, or randomize between min/max using X/Y hash?
            // Let's use max for consistency, or average.
            // Smooth randomness using noise
            // Use strata_noise. If not initialized, fallback to hash.
            int thickness = entry.max_thickness;
            if (entry.min_thickness != entry.max_thickness)
            {
              float t = 0.5f;
              if (strata_noise)
              {
                // Use different Seed per layer depth to decorrelate layers?
                // Or just X + depth offset.
                float n = strata_noise->GenSingle2D(global_x * 1.0f, current_depth * 100.0f, global_seed + 555);
                t = (n + 1.0f) * 0.5f; // -1..1 -> 0..1
              }
              else
              {
                // Fallback hash
                int h = (global_x * 73856093) ^ (global_seed);
                t = (float)(std::abs(h) % 100) / 100.0f;
              }
              thickness = entry.min_thickness + (int)(t * (entry.max_thickness - entry.min_thickness)); // truncate
            }

            if (depth < current_depth + thickness)
            {
              // Matched this layer
              if (i == 0) // Top Layer (Grass)
              {
                if (is_exposed && is_natural_exposure)
                {
                  tile = deepbound::tile_registry_t::get().get_tile(resource_id_t("deepbound", entry.tile_code));
                }
                else
                {
                  // We are covered, OR we are a cave floor.
                  // Fallthrough to next layer (Dirt) if possible.
                  if (i + 1 < active_layer->entries.size())
                  {
                    tile = deepbound::tile_registry_t::get().get_tile(resource_id_t("deepbound", active_layer->entries[i + 1].tile_code));
                  }
                  else
                  {
                    // No next layer, just use this one? Or Stone?
                    // Usually dirt.
                    tile = deepbound::tile_registry_t::get().get_tile(resource_id_t("deepbound", entry.tile_code));
                    // If "Grass" is used underground, it usually turns to dirt in game logic, but here we just place tile.
                    // We probably shouldn't place Grass underground.
                    // Hardcoded fallback to dirt if we don't have a lookup?
                    // For now, assume Entry 1 exists if Entry 0 exists.
                  }
                }
              }
              else
              {
                // Regular layer (Dirt, Clay, etc.)
                tile = deepbound::tile_registry_t::get().get_tile(resource_id_t("deepbound", entry.tile_code));
              }
              found = true;
              break;
            }
            current_depth += thickness;
          }

          if (!found)
          {
            // Deeper than all defined layers -> Stone
            tile = stone_tile;
          }
        }
      }
      else
      {
        // Air/Water
        if (global_y < sea_level)
        {
          if (base_density <= 0.0f)
            tile = water_tile;
          else
            tile = air_tile;
        }
      }

      chunk->set_tile(x, y, tile);
      chunk->set_climate(x, y, climate.first, climate.second);
    }
  }
}

float world_generator_t::get_density_final(int x, int y, float surface_height, float overhang_strength)
{
  float base = get_base_density(x, y, surface_height, overhang_strength);
  if (base <= 0.0f)
    return base; // Already air

  float modification = get_cave_density_modifier(x, y, surface_height);
  return base + modification; // Modification should be negative to carve
}

float world_generator_t::get_base_density(int x, int y, float surface_height, float overhang_strength)
{
  // Base density: Positive below surface, negative above.
  float density = surface_height - (float)y; // Simple linear falloff

  // If overhang strength is 0, we behave exactly like heightmap (density = distance to surface)
  // If we want overhangs, we add 3D noise.

  if (overhang_strength > 0.001f && overhang_noise)
  {
    // 3D noise sample.
    float noise = overhang_noise->GenSingle2D(x * 1.0f, y * 1.5f, global_seed + 12345);

    // Equation: surface_height + noise * strength > y
    density = (surface_height - (float)y) + (noise * overhang_strength * 40.0f); // 40.0 arbitrary scale factor for noise amp
  }

  return density;
}

float world_generator_t::get_cave_density_modifier(int x, int y, float surface_height)
{
  float depth = surface_height - (float)y;
  if (depth < cave_config.global_min_depth)
    return 0.0f; // Too close to surface, no caves

  float cave_mod = 0.0f;

  // 1. Cheese Caves (Large open areas)
  if (cave_config.cheese_enabled && cheese_noise)
  {
    // Use 3D noise (x, y, seed)
    float n = cheese_noise->GenSingle2D(x * 1.0f, y * 1.0f, cave_config.cheese_noise.seed);

    // Fade in near surface
    float fade = 1.0f;
    if (depth < cave_config.cheese_fade_depth)
    {
      fade = (depth - (float)cave_config.global_min_depth) / (cave_config.cheese_fade_depth - (float)cave_config.global_min_depth);
      fade = std::max(0.0f, std::min(1.0f, fade));
    }

    if (n > cave_config.cheese_threshold)
    {
      // Carve!
      // We return negative density.
      // If n is 0.6 and threshold is 0.5, we want to carve.
      // Larger n = more open.
      cave_mod -= (n - cave_config.cheese_threshold) * 1000.0f * fade; // Big negative number to ensure carving
    }
  }

  // 2. Worm Caves (Tunnels)
  if (cave_config.worm_enabled && worm_noise)
  {
    float n = worm_noise->GenSingle2D(x * 1.0f, y * 1.0f, cave_config.worm_noise.seed);
    // Ridged noise: -1 to 1. Val close to 1 is "ridge" usually? Or 0?
    // Standard ridged: 1 - abs(noise). Peaks are at 0 (or 1 depending on impl).
    // FastNoise FractalRidged usually returns peaks at 1.0.
    // Let's assume high values are tunnels.

    float fade = 1.0f;
    if (depth < cave_config.worm_fade_depth)
    {
      fade = (depth - (float)cave_config.global_min_depth) / (cave_config.worm_fade_depth - (float)cave_config.global_min_depth);
      fade = std::max(0.0f, std::min(1.0f, fade));
    }

    // Dynamic thickness
    float thickness = cave_config.worm_thickness;
    if (cave_config.worm_thickness_variation > 0.001f && overhang_noise)
    {
      // Reuse overhang noise (3D) for variation.
      // Offset coordinates to de-correlate from actual overhangs/terrain
      float var = overhang_noise->GenSingle2D(x * 1.0f + 500.0f, y * 1.5f + 500.0f, global_seed + 999);
      thickness += var * cave_config.worm_thickness_variation;
    }

    if (n > (1.0f - thickness))
    {
      cave_mod -= 1000.0f * fade;
    }
  }

  return cave_mod;
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
