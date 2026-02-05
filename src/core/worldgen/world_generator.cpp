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
  std::cout << "Cave Config Loaded." << std::endl;
}

void world_generator_t::load_provinces(const std::string &path)
{
  std::ifstream file(path);
  if (!file.is_open())
  {
    std::cerr << "Failed to open province config: " << path << std::endl;
    return;
  }

  nlohmann::json j;
  file >> j;

  if (j.contains("provinces"))
  {
    provinces.clear();
    for (const auto &p : j["provinces"])
    {
      GeologicalProvince prov;
      prov.name = p.value("name", "Unknown");
      prov.deep_stone_code = p.value("deep_stone", "rock-granite");

      // Resolve tile immediately
      prov.resolved_tile = deepbound::tile_registry_t::get().get_tile(resource_id_t("deepbound", prov.deep_stone_code));
      if (!prov.resolved_tile)
      {
        std::cout << "Warning: Could not resolve deep stone tile '" << prov.deep_stone_code << "' for province '" << prov.name << "'" << std::endl;
      }

      if (p.contains("layers"))
      {
        for (const auto &l : p["layers"])
        {
          ProvinceLayer layer;
          layer.tile_code = l.value("tile", "rock-granite");
          layer.type = l.value("type", "blob"); // blob, layer
          layer.frequency = l.value("frequency", 0.05f);
          layer.threshold = l.value("threshold", 0.6f);
          layer.resolved_tile = deepbound::tile_registry_t::get().get_tile(resource_id_t("deepbound", layer.tile_code));

          if (!layer.resolved_tile)
            std::cout << "Warning: Could not resolve mix tile '" << layer.tile_code << "' for province '" << prov.name << "'" << std::endl;

          prov.layers.push_back(layer);
        }
      }

      provinces.push_back(prov);
    }
  }

  // Province Noise Setup
  float frequency = 0.0005f;
  int seed_offset = 9999;
  if (j.contains("noise"))
  {
    frequency = j["noise"].value("frequency", 0.0005f);
    seed_offset = j["noise"].value("seed_offset", 9999);
  }

  auto signal = FastNoise::New<FastNoise::Simplex>();
  auto fractal = FastNoise::New<FastNoise::FractalFBm>();
  fractal->SetSource(signal);
  fractal->SetOctaveCount(2);
  fractal->SetGain(0.5f);
  fractal->SetLacunarity(2.0f);

  auto scale = FastNoise::New<FastNoise::DomainScale>();
  scale->SetSource(fractal);
  scale->SetScale(frequency);

  province_noise = scale;

  // Mix Noise (3D Simplex High Freq)
  {
    auto m_signal = FastNoise::New<FastNoise::Simplex>();
    auto m_fractal = FastNoise::New<FastNoise::FractalFBm>();
    m_fractal->SetSource(m_signal);
    m_fractal->SetOctaveCount(2);
    m_fractal->SetGain(0.5f);
    m_fractal->SetLacunarity(2.0f);

    auto m_scale = FastNoise::New<FastNoise::DomainScale>();
    m_scale->SetSource(m_fractal);
    m_scale->SetScale(0.05f); // Default mix scale

    province_mix_noise = m_scale;
  }

  std::cout << "Province Config Loaded. " << provinces.size() << " provinces." << std::endl;
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

          // Resolve tile
          layer.entries.back().resolved_tile = deepbound::tile_registry_t::get().get_tile(resource_id_t("deepbound", entry.tile_code));
          if (!layer.entries.back().resolved_tile)
          {
            // Try to find it, might be valid to be null if it's air?
            // But usually air is "air" code.
            // Warning only if not air
            if (entry.tile_code != "air")
              std::cout << "Warning: Could not resolve tile '" << entry.tile_code << "' for layer '" << layer.name << "'" << std::endl;
          }
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

          // Resolve tile
          layer.submerged_entries.back().resolved_tile = deepbound::tile_registry_t::get().get_tile(resource_id_t("deepbound", entry.tile_code));
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

// Optimization: Batch noise generation
// Optimization: Batch noise generation + Column Caching + Loop Interchange
void world_generator_t::generate_chunk(chunk_t *chunk, int chunk_x, int chunk_y)
{
  if (landforms.empty())
    return;

  // Constants
  const int SIZE = chunk_t::SIZE;
  const int global_x_start = chunk_x * SIZE;
  const int global_y_start = chunk_y * SIZE;

  // 1. Prepare Local Buffers (Stateless/Thread-Safe)
  // We use std::vector for safety/simplicity
  std::vector<float> overhang_map_buf(SIZE * SIZE);
  std::vector<float> cheese_map_buf(SIZE * SIZE);
  std::vector<float> worm_map_buf(SIZE * SIZE);
  std::vector<float> province_map_buf(SIZE * SIZE);
  std::vector<float> province_mix_map_buf(SIZE * SIZE);
  std::vector<float> strata_map_buf(SIZE * SIZE);

  std::vector<float> continental_map_buf(SIZE);
  std::vector<float> temp_map_buf(SIZE);
  std::vector<float> rain_map_buf(SIZE);

  std::vector<float> cached_surface_height(SIZE);
  std::vector<float> cached_overhang_strength(SIZE);
  std::vector<std::pair<float, float>> cached_climate(SIZE);
  std::vector<const BlockLayer *> cached_active_layer(SIZE);

  // 2. Generate Column Data (Stateless) --------------------------------------

  // Generate Column Noise
  if (continental_noise)
    continental_noise->GenUniformGrid2D(continental_map_buf.data(), global_x_start, 0, SIZE, 1, 1.0f, global_seed);

  if (temp_noise)
    temp_noise->GenUniformGrid2D(temp_map_buf.data(), global_x_start, 0, SIZE, 1, 1.0f, global_seed + 999);

  if (rain_noise)
    rain_noise->GenUniformGrid2D(rain_map_buf.data(), global_x_start, 0, SIZE, 1, 1.0f, global_seed + 888);

  // Derive Surface Height & Climate
  for (int x = 0; x < SIZE; x++)
  {
    int global_x = global_x_start + x;
    float cont_val = continental_map_buf[x];

    // Find landforms
    size_t i1 = 0, i2 = 0;
    float lf_t = 0.0f;

    if (cont_val <= landforms[0].threshold)
    {
      i1 = i2 = 0;
      lf_t = 0;
    }
    else if (cont_val >= landforms.back().threshold)
    {
      i1 = i2 = landforms.size() - 1;
      lf_t = 0;
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
          lf_t = (range > 0.0001f) ? (cont_val - landforms[i1].threshold) / range : 0;
          break;
        }
      }
    }

    // Height (Still scalar unfortunately for landforms, but fast enough)
    auto get_lf_h_local = [&](size_t idx)
    {
      const auto &lf = landforms[idx];
      float nv = 0.0f;
      if (idx < landform_noises.size() && landform_noises[idx])
        nv = landform_noises[idx]->GenSingle2D(global_x, 0, global_seed + 1337 + idx);
      return lf.base_height + (nv * lf.height_variance);
    };

    float h1 = get_lf_h_local(i1);
    float h2 = (i1 == i2) ? h1 : get_lf_h_local(i2);
    cached_surface_height[x] = h1 + (h2 - h1) * lf_t;

    // Overhang Strength
    cached_overhang_strength[x] = landforms[i1].overhang_strength * (1.0f - lf_t) + landforms[i2].overhang_strength * lf_t;

    // Climate
    float climate_t = temp_map_buf[x] * 30.0f + 10.0f;
    float climate_r = (rain_map_buf[x] + 1.0f) * 0.5f * 255.0f;
    cached_climate[x] = {climate_t, climate_r};

    // Active Layer (Pre-calculate!)
    const BlockLayer *active_layer = nullptr;
    for (const auto &layer : block_layers)
    {
      if (climate_t >= layer.min_temp && climate_t <= layer.max_temp && climate_r >= layer.min_rain && climate_r <= layer.max_rain)
      {
        active_layer = &layer;
        break;
      }
    }
    if (!active_layer && !block_layers.empty())
      active_layer = &block_layers[0];
    cached_active_layer[x] = active_layer;
  }

  // 3. Generate Chunk Maps (Batched) -----------------------------------------

  if (overhang_noise)
    overhang_noise->GenUniformGrid2D(overhang_map_buf.data(), global_x_start, global_y_start, SIZE, SIZE, 1.0f, global_seed + 12345);

  if (cave_config.cheese_enabled && cheese_noise)
    cheese_noise->GenUniformGrid2D(cheese_map_buf.data(), global_x_start, global_y_start, SIZE, SIZE, 1.0f, cave_config.cheese_noise.seed);

  if (cave_config.worm_enabled && worm_noise)
    worm_noise->GenUniformGrid2D(worm_map_buf.data(), global_x_start, global_y_start, SIZE, SIZE, 1.0f, cave_config.worm_noise.seed);

  if (!provinces.empty() && province_noise)
    province_noise->GenUniformGrid2D(province_map_buf.data(), global_x_start, global_y_start, SIZE, SIZE, 1.0f, global_seed + 9999);

  if (province_mix_noise)
    province_mix_noise->GenUniformGrid2D(province_mix_map_buf.data(), global_x_start, global_y_start, SIZE, SIZE, 1.0f, global_seed + 777);

  if (strata_noise)
    strata_noise->GenUniformGrid2D(strata_map_buf.data(), global_x_start, global_y_start, SIZE, SIZE, 1.0f, global_seed + 111);

  // 4. Process Chunk using Cached Data (Y-Outer Loop Optimization) -----------
  // Iterate Y first to access noise buffers linearly (row by row)
  for (int y = 0; y < SIZE; y++)
  {
    int global_y = global_y_start + y;

    for (int x = 0; x < SIZE; x++)
    {
      int global_x = global_x_start + x;
      int buf_idx = x + y * SIZE; // Linear access now!

      // Use Cache
      float surface_height = cached_surface_height[x];
      float overhang_strength = cached_overhang_strength[x];
      const BlockLayer *active_layer = cached_active_layer[x]; // Cached pointer

      // Map Lookups (Linear access)
      float noise_overhang_val = overhang_map_buf[buf_idx];
      float noise_cheese_val = cheese_map_buf[buf_idx];
      float noise_worm_val = worm_map_buf[buf_idx];
      float noise_province_val = province_map_buf[buf_idx];
      float noise_mix_val = province_mix_map_buf[buf_idx];
      float noise_strata_val = strata_map_buf[buf_idx];

      const tile_definition_t *tile = air_tile;

      // 1. Calculate Base Density
      float base_density = surface_height - (float)global_y;
      if (overhang_strength > 0.001f && overhang_noise)
      {
        base_density += (noise_overhang_val * overhang_strength * 40.0f);
      }

      // 2. Cave Modification
      float final_density = base_density;
      float cave_mod = 0.0f;
      if (base_density > 0.0f)
      {
        float depth = surface_height - (float)global_y;
        if (depth >= cave_config.global_min_depth)
        {
          // Cheese
          if (cave_config.cheese_enabled)
          {
            float fade = 1.0f;
            if (depth < cave_config.cheese_fade_depth) // Could optimize this branch out?
            {
              fade = (depth - (float)cave_config.global_min_depth) / (cave_config.cheese_fade_depth - (float)cave_config.global_min_depth);
              fade = std::max(0.0f, std::min(1.0f, fade));
            }
            if (noise_cheese_val > cave_config.cheese_threshold)
              cave_mod -= (noise_cheese_val - cave_config.cheese_threshold) * 1000.0f * fade;
          }

          // Worm
          if (cave_config.worm_enabled)
          {
            float fade = 1.0f;
            if (depth < cave_config.worm_fade_depth)
            {
              fade = (depth - (float)cave_config.global_min_depth) / (cave_config.worm_fade_depth - (float)cave_config.global_min_depth);
              fade = std::max(0.0f, std::min(1.0f, fade));
            }
            float thickness = cave_config.worm_thickness;
            if (noise_worm_val > (1.0f - thickness))
              cave_mod -= 1000.0f * fade;
          }
        }
        final_density += cave_mod;
      }

      // 3. Tile Decision
      if (final_density > 0.0f)
      {
        // Solid
        // Check "Above" for exposure.
        bool is_exposed = false;
        bool is_natural_exposure = false;

        float base_density_above;
        float final_density_above;

        if (y + 1 < SIZE)
        {
          // Use buffer (Next Row, same X) -> buf_idx + SIZE
          float n_ov_up = overhang_map_buf[buf_idx + SIZE];

          base_density_above = surface_height - (float)(global_y + 1);
          if (overhang_strength > 0.001f && overhang_noise)
            base_density_above += (n_ov_up * overhang_strength * 40.0f);

          float c_mod_up = 0.0f;
          if (base_density_above > 0.0f)
          {
            float n_ch_up = cheese_map_buf[buf_idx + SIZE];
            float n_wm_up = worm_map_buf[buf_idx + SIZE];
            float d_up = surface_height - (float)(global_y + 1);
            if (d_up >= cave_config.global_min_depth)
            {
              if (cave_config.cheese_enabled && n_ch_up > cave_config.cheese_threshold)
                c_mod_up -= 100.0f;
              if (cave_config.worm_enabled && n_wm_up > (1.0f - cave_config.worm_thickness))
                c_mod_up -= 100.0f;
            }
          }
          final_density_above = base_density_above + c_mod_up;
        }
        else
        {
          // Fallback for chunk boundary
          final_density_above = get_density_final(global_x, global_y + 1, surface_height, overhang_strength);
          base_density_above = get_base_density(global_x, global_y + 1, surface_height, overhang_strength);
        }

        if (final_density_above <= 0.0f)
        {
          is_exposed = true;
          if (base_density_above <= 0.0f)
            is_natural_exposure = true;
        }

        int depth = (int)(surface_height - (float)global_y);
        if (depth < 0)
          depth = 0;

        // Resolve Deep Stone
        const tile_definition_t *deep_stone = stone_tile;
        if (!provinces.empty())
        {
          float pv = noise_province_val;
          // Distortion
          if (strata_noise)
            pv += noise_strata_val * 0.15f;

          float norm = (pv + 1.0f) * 0.5f;
          norm = std::max(0.0f, std::min(0.99f, norm));
          int p_idx = (int)(norm * provinces.size());

          // Use CACHED tile
          if (provinces[p_idx].resolved_tile)
            deep_stone = provinces[p_idx].resolved_tile;

          // Check Mixtures/Layers
          const auto &prov = provinces[p_idx];
          for (const auto &layer : prov.layers)
          {
            if (!layer.resolved_tile)
              continue;

            bool applies = false;
            if (layer.type == "blob")
            {
              // Use mix noise (3D-ish, but here projected 2D from same buffer? No, GenUniformGrid2D generates XY plane)
              // We effectively have 2D noise in the buffer.
              // For TRUE 3D blobs we need distinct Y sampling or a 3D buffer.
              // Current architecture generates 2D buffers per chunk (which is 2D vertical slice? No, chunk_t is 32x32?)
              // Chunk is likely 32x32 blocks?
              // Wait, check generate_chunk signature: chunk_x, chunk_y.
              // If this is a side-view 2D game (Terraria-like), then X and Y are world coordinates.
              // The GenUniformGrid2D is called with (global_x_start, global_y_start).
              // So "province_mix_map_buf" DOES contain unique noise for this X,Y block.
              // So noise_mix_val IS effectively coherent 2D noise for this pixel. Perfect.

              // However, we might want different seeds for different layers?
              // For simplicity, we stick to one master mix noise for now, using thresholds.
              // Or we could offset the value based on layer index: sin(val + index) etc.

              // Simple Threshold:
              float n = noise_mix_val;
              // To avoid all layers appearing in same spot, offset noise by layer index
              // Cheap offset:
              n = std::fmod(n + (float)(&layer - &prov.layers[0]) * 0.31f, 1.0f);
              if (n > layer.threshold)
                applies = true;
            }
            else if (layer.type == "layer" || layer.type == "strata")
            {
              // Y-dependent mixing.
              // Use strata noise (low freq) + local Y
              // A layer typically appears at certain depths or repeating intervals.
              // Let's do repeating intervals for "banding"
              // band = sin(y * freq + noise)
              float band = std::sin((float)global_y * 0.1f * layer.frequency + noise_strata_val * 4.0f);
              if (band > layer.threshold)
                applies = true;
            }
            else if (layer.type == "vein") // Rare veins
            {
              float n = std::sin(noise_mix_val * 10.0f + (float)global_y * 0.1f);
              if (n > layer.threshold + 0.2f)
                applies = true;
            }
            else if (layer.type == "dyke") // Vertical intrusions
            {
              // X-dependent noise. We need to create vertical bands.
              // Band = sin(x * freq + noise)
              // Use noise_mix_val (which is xy) to distort the bands so they aren't perfect lines.
              float band = std::sin((float)global_x * 0.1f * layer.frequency + noise_mix_val * 2.0f);
              // Sharp threshold for dykes
              if (band > layer.threshold)
                applies = true;
            }
            else if (layer.type == "crust") // Surface coating
            {
              // Depth dependent
              if (depth < (int)(layer.frequency * 100.0f)) // Reuse frequency as depth scale (0.1 = 10 blocks, etc.)
              {
                // Optional noise breakup
                if (noise_mix_val > layer.threshold)
                  applies = true;
              }
            }

            if (applies)
            {
              deep_stone = layer.resolved_tile;
              // Don't break? Or break?
              // If we don't break, subsequent layers generate "on top".
              // Let's break to keep it simple order-based priority.
              // break;
              // Actually, "layers" should probably override "blobs"?
              // If we iterate array order, last one wins.
            }
          }
        }

        tile = deep_stone;

        if (active_layer && !active_layer->entries.empty())
        {
          int current_depth = 0;
          bool found_layer = false;
          for (size_t i = 0; i < active_layer->entries.size(); i++)
          {
            const auto &entry = active_layer->entries[i];
            int thickness = entry.max_thickness;
            if (entry.min_thickness != entry.max_thickness)
            {
              // Variation logic (skip for perf or use hash)
              int h = (global_x * 73856093) ^ (global_seed + current_depth * 99); // Simple hash
              float t = (float)(std::abs(h) % 100) / 100.0f;
              thickness = entry.min_thickness + (int)(t * (entry.max_thickness - entry.min_thickness));
            }

            if (depth < current_depth + thickness)
            {
              // Matched
              if (i == 0 && is_exposed && is_natural_exposure)
              {
                if (entry.resolved_tile)
                  tile = entry.resolved_tile;
              }
              else
              {
                // Covered or cave floor
                if (i == 0) // If it was grass but covered
                {
                  if (i + 1 < active_layer->entries.size())
                  {
                    if (active_layer->entries[i + 1].resolved_tile)
                      tile = active_layer->entries[i + 1].resolved_tile;
                  }
                  else
                  {
                    if (entry.resolved_tile)
                      tile = entry.resolved_tile; // Fallback
                  }
                }
                else
                {
                  if (entry.resolved_tile)
                    tile = entry.resolved_tile;
                }
              }
              found_layer = true;
              break;
            }
            current_depth += thickness;
          }
          if (!found_layer)
            tile = deep_stone;
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
      // chunk->set_climate(x, y, climate_t, climate_r); // Climate is not Y-dependent, so we can access cached
      chunk->set_climate(x, y, cached_climate[x].first, cached_climate[x].second);
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
