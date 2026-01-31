#include "core/worldgen/world_generator.hpp"
#include "core/assets/json_loader.hpp"
#include "core/common/resource_id.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <map>
#include <random>

namespace deepbound
{

const resource_id_t world_generator_t::AIR_ID("deepbound:air");
const resource_id_t world_generator_t::WATER_ID("deepbound:water");

world_generator_t::world_generator_t() : m_noise(0), m_temp_noise(0), m_rain_noise(0), m_province_noise(0), m_strata_noise(0), m_upheaval_noise(0)
{
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(1, 1000000); // 1M range is sufficient for FastNoise seeds usually

  int master_seed = dis(gen);
  std::cout << "WorldGen Seed: " << master_seed << std::endl;

  // Derive other seeds deterministically from the master seed, or randomly
  // It's often better to derive them so one seed reproduces everything.
  // Simple LCG or just offset
  m_noise.set_seed(master_seed);
  m_temp_noise.set_seed(master_seed + 123);
  m_rain_noise.set_seed(master_seed + 456);
  m_province_noise.set_seed(master_seed + 789);
  m_strata_noise.set_seed(master_seed + 4242);
  m_upheaval_noise.set_seed(master_seed + 999);

  init_context();
}

// Simple integer hash
static auto int_noise(int x, int y, int seed) -> int
{
  int n = x * 1619 + y * 31337 + seed * 1013;
  n = (n << 13) ^ n;
  return (n * (n * n * 60493 + 19990303) + 1376312589);
}

static auto get_random_float(int x, int y, int seed) -> float
{
  int val = int_noise(x, y, seed) & 0x7fffffff;
  return (float)val / 2147483648.0f;
}

auto world_generator_t::init_context() -> void
{
  if (m_initialized)
    return;

  std::string assets_path = "assets/worldgen";
  json_loader_t::load_worldgen(assets_path, m_context);

  std::cout << "WorldGen Initialized. Loaded " << m_context.landforms.size() << " landforms, " << m_context.rock_strata.size() << " strata, " << m_context.geologic_provinces.size() << " provinces." << std::endl;
  m_initialized = true;
}

auto world_generator_t::get_landform_weights(float x, float y, std::vector<landform_weight_t> &out_weights) -> void
{
  out_weights.clear();

  // Calculate climate for this position
  float temp = m_temp_noise.get_noise(x * 0.0001f, 0) * 50.0f;
  float rain = (m_rain_noise.get_noise(x * 0.0001f, 0) + 1.0f) * 128.0f;

  // Determine candidate landforms based on climate
  // Determine candidate landforms based on climate
  float total_candidate_weight = 0.0f;

  static thread_local std::vector<const landform_variant_t *> candidates;
  candidates.clear();

  for (const auto &lf : m_context.landforms)
  {
    if (lf.use_climate)
    {
      if (temp >= lf.min_temp && temp <= lf.max_temp && rain >= lf.min_rain && rain <= lf.max_rain)
      {
        candidates.push_back(&lf);
        total_candidate_weight += lf.weight;
      }
    }
    else
    {
      candidates.push_back(&lf);
      total_candidate_weight += lf.weight;
    }
  }

  if (candidates.empty())
  {
    // Fallback to a default landform if no candidates match
    if (!m_context.landforms.empty())
    {
      out_weights.push_back({&m_context.landforms[0], 1.0f});
    }
    return;
  }

  // Landform scale is 256.
  constexpr float scale = 256.0f;

  // Apply wobble to get distorted coordinates
  float wobble_freq = 0.002f;
  float wobble_mag = 400.0f;
  float wx = x + m_noise.get_noise(x * wobble_freq, y * wobble_freq) * wobble_mag;
  float wy = y + m_noise.get_noise(y * wobble_freq, x * wobble_freq + 1000) * wobble_mag;

  // We need to sample the 4 nearest grid centers for bilinear interpolation.
  // The grid nodes are at integer coordinates of (wx/scale, wy/scale).
  // Let u = wx/scale, v = wy/scale.
  float u = wx / scale;
  float v = wy / scale;

  // Grid cell coordinates (top-left)
  // To match VS "centers", we can treat integer coords as centers.
  // Sampling at (u, v) means we are between floor(u) and floor(u)+1.

  // Shift by 0.5 to align with pixel centers if needed, but let's stick to simple grid interpolation.
  // We want to interpolate between (floor(u), floor(v)), (ceil(u), floor(v)), etc.

  // Actually, VS creates a map and then interpolates. Here we compute procedurally.
  // Centers are at (floor(u-0.5), floor(v-0.5)) etc?
  // Let's use standard bilinear interpolation logic.
  // Top-Left corner:
  int x0 = (int)std::floor(u - 0.5f);
  int y0 = (int)std::floor(v - 0.5f);
  int x1 = x0 + 1;
  int y1 = y0 + 1;

  // Local blend factors (0.0 to 1.0)
  float s = (u - 0.5f) - x0;
  float t = (v - 0.5f) - y0;

  // Helper to get landform for a specific grid coordinate
  auto get_lf = [&](int gx, int gy)
  {
    auto seed = m_noise.get_seed();
    float r = get_random_float(gx, gy, seed) * total_candidate_weight;
    float current_weight = 0.0f;
    for (const auto *lf : candidates)
    {
      current_weight += lf->weight;
      if (r <= current_weight)
        return lf;
    }
    return candidates.back();
  };

  // Collect 4 samples
  const auto *lf00 = get_lf(x0, y0);
  const auto *lf10 = get_lf(x1, y0);
  const auto *lf01 = get_lf(x0, y1);
  const auto *lf11 = get_lf(x1, y1);

  // Calculate weights
  // 00: (1-s)(1-t)
  // 10: s(1-t)
  // 01: (1-s)t
  // 11: st

  auto add_weight = [&](const landform_variant_t *lf, float w)
  {
    if (w <= 0.001f)
      return; // Ignore very small weights
    for (auto &entry : out_weights)
    {
      if (entry.landform == lf)
      {
        entry.weight += w;
        return;
      }
    }
    out_weights.push_back({lf, w});
  };

  add_weight(lf00, (1.0f - s) * (1.0f - t));
  add_weight(lf10, s * (1.0f - t));
  add_weight(lf01, (1.0f - s) * t);
  add_weight(lf11, s * t);
}
// get_landform is kept for compatibility/debug but internal logic usually wants weights now.
// We can implement it as returning the highest weight one from get_landform_weights.
auto world_generator_t::get_landform(float x, float y) -> const landform_variant_t *
{
  std::vector<landform_weight_t> weights;
  get_landform_weights(x, y, weights);
  if (weights.empty())
    return &m_context.landforms[0];

  const landform_variant_t *best = weights[0].landform;
  float best_w = weights[0].weight;
  for (size_t i = 1; i < weights.size(); ++i)
  {
    if (weights[i].weight > best_w)
    {
      best_w = weights[i].weight;
      best = weights[i].landform;
    }
  }
  return best;
}

auto world_generator_t::prepare_column_data(float x, float z) -> column_data_t
{
  column_data_t data;
  get_landform_weights(x, z, data.weights);
  data.max_noise_amp = 0.0f;

  if (!data.weights.empty())
  {
    size_t oct_size = data.weights[0].landform->terrain_octaves.size();
    size_t th_size = data.weights[0].landform->terrain_octave_thresholds.size();

    data.blended_octaves.assign(oct_size, 0.0);
    data.blended_thresholds.assign(th_size, 0.0);

    for (const auto &w : data.weights)
    {
      for (size_t i = 0; i < std::min(data.blended_octaves.size(), w.landform->terrain_octaves.size()); ++i)
      {
        data.blended_octaves[i] += w.landform->terrain_octaves[i] * w.weight;
      }
      // If the landform has thresholds, blend them. Otherwise assume 0.
      if (!w.landform->terrain_octave_thresholds.empty())
      {
        for (size_t i = 0; i < std::min(data.blended_thresholds.size(), w.landform->terrain_octave_thresholds.size()); ++i)
        {
          data.blended_thresholds[i] += w.landform->terrain_octave_thresholds[i] * w.weight;
        }
      }
    }

    // Calculate max noise amplitude for bounding
    for (double val : data.blended_octaves)
    {
      data.max_noise_amp += (float)std::abs(val);
    }

    // surface_noise is no longer calculated here as it is now part of the density function
    data.surface_noise = 0.0f;
  }
  else
  {
    data.surface_noise = 0.0f;
  }

  data.upheaval = m_upheaval_noise.get_noise(x * 0.0005f, 555) * 0.1f;
  return data;
}

auto world_generator_t::get_density_from_column(float x, float y, const column_data_t &data) -> float
{
  float normalized_y = y / (float)m_world_height;
  float blended_y_offset = 0.0f;
  for (const auto &w : data.weights)
  {
    blended_y_offset += w.landform->y_key_thresholds.evaluate(normalized_y) * w.weight;
  }

  float base_density = (blended_y_offset - 0.5f) * 6.0f + data.upheaval;

  // Optimization: Early-out if noise cannot physically change the air/solid state
  // Using 1.0 margin as get_terrain_noise is normalized approx -1..1
  if (base_density > 1.0f)
    return 1.0f; // Definitely solid
  if (base_density < -1.0f)
    return -1.0f; // Definitely air

  // Use the blended landform noise configuration for the detailed density noise
  // This allows the noise character to change based on the landform (smooth vs jagged)
  float density_noise = 0.0f;
  if (!data.blended_octaves.empty())
  {
    density_noise = m_noise.get_terrain_noise(x, y, data.blended_octaves, data.blended_thresholds);
  }

  return base_density + density_noise;
}

float world_generator_t::get_density(float x, float y, float z)
{
  if (!m_initialized)
    return 0.0f;

  // Fallback to slow full calculation
  auto data = prepare_column_data(x, z);
  return get_density_from_column(x, y, data);
}

auto world_generator_t::get_province(float x, float y) -> const geologic_province_variant_t *
{
  if (m_context.geologic_provinces.empty())
    return nullptr;

  // Provinces match VS: ~4096 blocks (64 * 64).
  float wobble_freq = 0.0005f;
  float wobble_mag = 2000.0f;

  float wx = x + m_province_noise.get_noise(x * wobble_freq, y * wobble_freq) * wobble_mag;
  float wy = y + m_province_noise.get_noise(y * wobble_freq, x * wobble_freq + 1000) * wobble_mag;

  float scale = 4096.0f;
  int px = (int)std::floor(wx / scale);
  int py = (int)std::floor(wy / scale);

  float normalized = get_random_float(px, py, m_province_noise.get_seed());

  size_t index = (size_t)(normalized * m_context.geologic_provinces.size());
  if (index >= m_context.geologic_provinces.size())
    index = m_context.geologic_provinces.size() - 1;

  // VS uses weighted list for provinces too (NoiseGeoProvince.cs).
  // I should check weights. But checking JSON for provinces...
  // Assuming equal weights for now or relying on index mapping if weight sum is not used.
  // Wait, NoiseGeoProvince uses weights. I need to iterate weights.
  // Let's implement weighted selection for provinces.

  if (!m_context.geologic_provinces.empty())
  {
    float total_weight = 0.0f;
    for (const auto &p : m_context.geologic_provinces)
      total_weight += p.weight;

    float r = normalized * total_weight;
    float current = 0.0f;
    for (const auto &p : m_context.geologic_provinces)
    {
      current += p.weight;
      if (r <= current)
        return &p;
    }
    return &m_context.geologic_provinces.back();
  }

  return &m_context.geologic_provinces[index];
}

auto world_generator_t::get_rock_strata(float x, float y, float density, const geologic_province_variant_t *province) -> std::string
{
  return "deepbound:rock-granite";
}

auto world_generator_t::generate_chunk(int chunk_x, int chunk_y) -> std::unique_ptr<chunk_t>
{
  auto chunk = std::make_unique<chunk_t>(chunk_x, chunk_y);

  int world_x_base = chunk_x * CHUNK_SIZE;
  int world_y_base = chunk_y * CHUNK_SIZE;

  int prev_surface_y = m_world_height / 2;

  for (int x = 0; x < CHUNK_SIZE; ++x)
  {
    int wx = world_x_base + x;

    std::shared_ptr<cached_column_info_t> col_info_ptr;

    // Use Shard 0-31 based on World X
    auto &shard = m_column_caches[wx & 31];
    {
      std::lock_guard<std::mutex> lock(shard.mutex);
      auto it = shard.map.find(wx);
      if (it != shard.map.end())
      {
        col_info_ptr = it->second;
      }
    }

    if (!col_info_ptr)
    {
      col_info_ptr = std::make_shared<cached_column_info_t>();

      // 1. Prepare Data
      col_info_ptr->data = prepare_column_data((float)wx, 0.0f);

      // 2. Surface Find
      int surface_y = 0;
      int start_y = std::min(m_world_height - 1, std::max(0, prev_surface_y + 32));

      for (int sy = start_y; sy >= 0; --sy)
      {
        if (get_density_from_column((float)wx, (float)sy, col_info_ptr->data) > 0.0f)
        {
          surface_y = sy;
          break;
        }
      }
      col_info_ptr->surface_y = surface_y;

      // 3. Strata Generation
      const geologic_province_variant_t *current_province = get_province((float)wx, 0);

      static thread_local std::vector<std::pair<std::string, float>> rock_usage;
      rock_usage.clear();

      int ylower = 0;
      int yupper = surface_y;
      std::string last_bu_code = "";

      for (const auto &stratum : m_context.rock_strata)
      {
        std::vector<float> scaled_freqs = stratum.frequencies;
        for (auto &f : scaled_freqs)
          f *= 0.1f;

        float thickness_raw = m_noise.get_custom_noise((float)wx, 0.0f, stratum.amplitudes, stratum.thresholds, scaled_freqs);
        float thickness = thickness_raw * 10.0f + 20.0f;

        float max_allowed = 999.0f;
        if (current_province)
        {
          auto it = current_province->rock_strata_thickness.find(stratum.rock_group);
          if (it != current_province->rock_strata_thickness.end())
          {
            max_allowed = it->second * 2.0f;
          }
        }

        float used = 0.0f;
        for (const auto &p : rock_usage)
        {
          if (p.first == stratum.rock_group)
          {
            used = p.second;
            break;
          }
        }

        float allowed = max_allowed - used;
        float actual_thickness = std::min(thickness, std::max(0.0f, allowed));

        if (actual_thickness >= 2.0f)
        {
          if (stratum.gen_dir == "TopDown")
          {
            col_info_ptr->strata_ranges.push_back({"deepbound:" + stratum.block_code, (int)(yupper - actual_thickness), yupper});
            yupper -= (int)actual_thickness;
          }
          else
          {
            col_info_ptr->strata_ranges.push_back({"deepbound:" + stratum.block_code, ylower, (int)(ylower + actual_thickness)});
            ylower += (int)actual_thickness;
            last_bu_code = stratum.block_code;
          }

          bool found_r = false;
          for (auto &p : rock_usage)
          {
            if (p.first == stratum.rock_group)
            {
              p.second += actual_thickness;
              found_r = true;
              break;
            }
          }
          if (!found_r)
            rock_usage.push_back({stratum.rock_group, actual_thickness});
        }
      }
      col_info_ptr->last_bottom_up_code = last_bu_code;

      {
        std::lock_guard<std::mutex> lock(shard.mutex);
        shard.map[wx] = col_info_ptr;
      }
    }

    prev_surface_y = col_info_ptr->surface_y;

    // 4. Block Filling
    for (int y = 0; y < CHUNK_SIZE; ++y)
    {
      int wy = world_y_base + y;

      if (wy >= m_world_height)
      {
        chunk->set_tile(x, y, AIR_ID);
        continue;
      }

      float density = get_density_from_column((float)wx, (float)wy, col_info_ptr->data);

      if (density > 0.0f)
      {
        std::string rock_type = "deepbound:rock-obsidian";

        float b_noise = m_noise.get_noise((float)wx * 0.02f, (float)wy * 0.02f) * 2.0f;

        bool found = false;
        for (const auto &range : col_info_ptr->strata_ranges)
        {
          if (wy >= (float)range.y_min + b_noise && wy <= (float)range.y_max + b_noise)
          {
            rock_type = range.code;
            found = true;
            break;
          }
        }

        if (!found && !col_info_ptr->last_bottom_up_code.empty())
        {
          rock_type = "deepbound:" + col_info_ptr->last_bottom_up_code;
        }

        chunk->set_tile(x, y, {rock_type}); // Still allocs temp string for rock types (less common than Air/Water)
      }
      else
      {
        if (wy < m_sea_level)
          chunk->set_tile(x, y, WATER_ID);
        else
          chunk->set_tile(x, y, AIR_ID);
      }
    }
  }

  return chunk;
}

} // namespace deepbound
