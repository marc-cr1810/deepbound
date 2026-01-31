#include "core/assets/json_loader.hpp"
#include "core/content/tile.hpp"
#include "core/worldgen/world_gen_context.hpp"
#include "core/common/resource_id.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

#include <regex>

namespace deepbound
{

// Internal helper to handle loose JSON (comments, unquoted keys, trailing commas)
auto standardize_json(const std::string &input) -> std::string
{
  // 1. Strip single-line comments //
  std::string result = std::regex_replace(input, std::regex("//.*"), "");

  // 2. Fix trailing commas before closing braces/brackets
  result = std::regex_replace(result, std::regex(",\\s*([}\\]])"), "$1");

  // 3. Quote unquoted keys
  // Focus on keys at start of line or after { ,
  result = std::regex_replace(result, std::regex("([{,])\\s*([a-zA-Z_][a-zA-Z0-9_]*)\\s*:"), "$1\"$2\":");
  result = std::regex_replace(result, std::regex("(^|\\n)\\s*([a-zA-Z_][a-zA-Z0-9_]*)\\s*:"), "$1\"$2\":");

  return result;
}

auto json_loader_t::load_tiles_from_directory(const std::string &directory_path) -> void
{
  // Register basic tiles that generator depends on early
  // This ensures they are available for atlas building
  tile_definition_t air;
  air.code = "air";
  air.id = resource_id_t("deepbound", "air");
  tile_registry_t::get().register_tile(air);

  if (!std::filesystem::exists(directory_path))
  {
    std::cerr << "Warning: Tile directory not found: " << directory_path << std::endl;
    return;
  }

  // Use recursive_directory_iterator to find tiles in subdirectories (stone, soil, liquid, etc)
  for (const auto &entry : std::filesystem::recursive_directory_iterator(directory_path))
  {
    if (entry.is_regular_file() && entry.path().extension() == ".json")
    {
      std::ifstream f(entry.path());
      if (!f.is_open())
        continue;
      std::stringstream buffer;
      buffer << f.rdbuf();
      parse_and_register_tile(buffer.str(), entry.path().filename().string());
    }
  }
}

auto json_loader_t::parse_and_register_tile(const std::string &json_content, const std::string &filename) -> void
{
  try
  {
    auto j = nlohmann::json::parse(standardize_json(json_content), nullptr, true, true);

    tile_definition_t base_def;
    base_def.code = j.value("code", "unknown");
    base_def.id = resource_id_t("deepbound", base_def.code);

    if (j.contains("textures"))
    {
      for (auto &[key, val] : j["textures"].items())
      {
        base_def.textures[key] = resource_id_t("deepbound", val.get<std::string>());
      }
    }

    // Determine states for variants
    std::string placeholder = "";
    std::vector<std::string> states;

    if (base_def.code == "rock")
    {
      placeholder = "{type}";
      states = {"basalt", "limestone", "sandstone", "granite"};
    }
    else if (base_def.code == "soil")
    {
      placeholder = "{fertility}";
      states = {"none", "low", "medium", "high"};
    }

    // If we have variants, register them. If not, just register the base.
    if (!states.empty())
    {
      for (const auto &s : states)
      {
        tile_definition_t var = base_def;
        var.code = base_def.code + "-" + s;
        var.id = resource_id_t("deepbound", var.code);

        for (auto &[k, v] : var.textures)
        {
          std::string p = v.get_path();
          size_t pos = p.find(placeholder);
          if (pos != std::string::npos)
          {
            p.replace(pos, placeholder.length(), s);
          }
          var.textures[k] = resource_id_t("deepbound", p);
        }
        tile_registry_t::get().register_tile(var);
      }
    }
    else
    {
      tile_registry_t::get().register_tile(base_def);
    }
  }
  catch (const std::exception &e)
  {
    std::cerr << "Failed to parse tile " << filename << ": " << e.what() << std::endl;
  }
}

auto json_loader_t::load_strata_from_file(const std::string &file_path) -> void
{
  // Implementation placeholder
}

auto json_loader_t::parse_strata_json(const std::string &json_content) -> void
{
  // Implementation placeholder
}

auto json_loader_t::load_worldgen(const std::string &base_dir, world_gen_context_t &context) -> void
{
  auto load_file = [](const std::string &path) -> nlohmann::json
  {
    if (!std::filesystem::exists(path))
      return nlohmann::json();

    try
    {
      std::ifstream f(path);
      std::stringstream buffer;
      buffer << f.rdbuf();
      std::string content = standardize_json(buffer.str());
      // Use parse with ignore_comments = true just in case standardize missed some
      return nlohmann::json::parse(content, nullptr, true, true);
    }
    catch (const std::exception &e)
    {
      std::cerr << "JSON Parse Error in " << path << ": " << e.what() << std::endl;
      return nlohmann::json();
    }
  };

  // Load Landforms
  nlohmann::json j_landforms = load_file(base_dir + "/landforms.json");
  if (j_landforms.contains("variants"))
  {
    for (const auto &var : j_landforms["variants"])
    {
      landform_variant_t lf;
      lf.code = var.value("code", "unknown");
      lf.hexcolor = var.value("hexcolor", "#FFFFFF");
      lf.weight = var.value("weight", 1.0f);
      lf.use_climate = var.value("useClimateMap", false);
      lf.min_temp = var.value("minTemp", -50.0f);
      lf.max_temp = var.value("maxTemp", 50.0f);
      lf.min_rain = var.value("minRain", 0.0f);
      lf.max_rain = var.value("maxRain", 255.0f);

      if (var.contains("terrainOctaves"))
        lf.terrain_octaves = var["terrainOctaves"].get<std::vector<double>>();

      if (var.contains("terrainYKeyPositions"))
        lf.y_key_thresholds.keys = var["terrainYKeyPositions"].get<std::vector<float>>();

      if (var.contains("terrainYKeyThresholds"))
        lf.y_key_thresholds.values = var["terrainYKeyThresholds"].get<std::vector<float>>();

      context.landforms.push_back(lf);
    }
  }

  // Load Rock Strata
  nlohmann::json j_strata = load_file(base_dir + "/rockstrata.json");
  if (j_strata.contains("variants"))
  {
    for (const auto &var : j_strata["variants"])
    {
      rock_strata_variant_t rs;
      rs.block_code = var.value("blockcode", "rock-granite");
      rs.rock_group = var.value("rockGroup", "Igneous");
      rs.gen_dir = var.value("genDir", "BottomUp");

      if (var.contains("amplitudes"))
        rs.amplitudes = var["amplitudes"].get<std::vector<float>>();
      if (var.contains("thresholds"))
        rs.thresholds = var["thresholds"].get<std::vector<float>>();
      if (var.contains("frequencies"))
        rs.frequencies = var["frequencies"].get<std::vector<float>>();

      context.rock_strata.push_back(rs);
    }
  }

  // Load Geologic Provinces
  nlohmann::json j_prov = load_file(base_dir + "/geologicprovinces.json");
  if (j_prov.contains("variants"))
  {
    for (const auto &var : j_prov["variants"])
    {
      geologic_province_variant_t gp;
      gp.code = var.value("code", "unknown");
      gp.weight = var.value("weight", 10.0f);

      if (var.contains("rockstrata"))
      {
        for (auto &[key, val] : var["rockstrata"].items())
        {
          gp.rock_strata_thickness[key] = val.value("maxThickness", 0.0f);
        }
      }
      context.geologic_provinces.push_back(gp);
    }
  }
}

} // namespace deepbound
