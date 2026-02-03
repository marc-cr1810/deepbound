#include "core/assets/json_loader.hpp"
#include "core/content/tile.hpp"
// #include "core/worldgen/world_gen_context.hpp"
#include "core/common/resource_id.hpp"
#include "core/assets/asset_manager.hpp"
#include "core/worldgen/world_generator.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

#include <regex>
#include <functional>

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

    if (j.contains("drawtype"))
    {
      base_def.draw_type = j["drawtype"];
    }

    if (j.contains("textures"))
    {
      for (auto &[key, val] : j["textures"].items())
      {
        if (key == "specialSecondTexture")
        {
          if (val.is_string())
          {
            base_def.special_second_texture = resource_id_t("deepbound", val.get<std::string>());
          }
          else if (val.contains("base"))
          {
            base_def.special_second_texture = resource_id_t("deepbound", val["base"].get<std::string>());
          }
          continue;
        }

        if (val.is_string())
        {
          base_def.textures[key] = resource_id_t("deepbound", val.get<std::string>());
        }
        else if (val.contains("base"))
        {
          base_def.textures[key] = resource_id_t("deepbound", val["base"].get<std::string>());
        }
      }
    }

    if (j.contains("overlays"))
    {
      for (const auto &val : j["overlays"])
      {
        base_def.overlays.push_back(resource_id_t("deepbound", val.get<std::string>()));
      }
    }

    if (j.contains("climateColorMap"))
    {
      base_def.climate_color_map = j["climateColorMap"].get<std::string>();
    }

    // Determine variants
    struct variant_group_t
    {
      std::string code;
      std::vector<std::string> states;
    };
    std::vector<variant_group_t> groups;

    if (j.contains("variantgroups"))
    {
      for (const auto &group : j["variantgroups"])
      {
        if (group.contains("states") && group.contains("code"))
        {
          variant_group_t g;
          g.code = group["code"].get<std::string>();
          for (const auto &s : group["states"])
          {
            g.states.push_back(s.get<std::string>());
          }
          groups.push_back(g);
        }
      }
    }

    if (!groups.empty())
    {
      // Recursive lambda to generate combinations
      std::function<void(size_t, std::string, std::vector<std::pair<std::string, std::string>>)> generate_combinations;
      generate_combinations = [&](size_t group_idx, std::string current_suffix, std::vector<std::pair<std::string, std::string>> replacements)
      {
        if (group_idx >= groups.size())
        {
          // Finalize variant
          tile_definition_t var = base_def;
          var.code = base_def.code + current_suffix;
          var.id = resource_id_t("deepbound", var.code);

          // Apply replacements to all textures
          for (auto &[k, v] : var.textures)
          {
            std::string p = v.get_path();
            for (const auto &rep : replacements)
            {
              std::string ph = "{" + rep.first + "}";
              size_t pos = 0;
              while ((pos = p.find(ph, pos)) != std::string::npos)
              {
                p.replace(pos, ph.length(), rep.second);
                pos += rep.second.length();
              }
            }
            var.textures[k] = resource_id_t("deepbound", p);
          }

          // Apply replacements to special_second_texture
          if (!var.special_second_texture.get_path().empty())
          {
            std::string p = var.special_second_texture.get_path();
            for (const auto &rep : replacements)
            {
              std::string ph = "{" + rep.first + "}";
              size_t pos = 0;
              while ((pos = p.find(ph, pos)) != std::string::npos)
              {
                p.replace(pos, ph.length(), rep.second);
                pos += rep.second.length();
              }
            }
            var.special_second_texture = resource_id_t("deepbound", p);
          }

          // Helper to check wildcard match for specific properties
          auto check_map_property = [&](const nlohmann::json &json_map, const std::string &suffix) -> std::string
          {
            for (auto it = json_map.begin(); it != json_map.end(); ++it)
            {
              std::string key = it.key();
              // formatting: *-none, *-normal. Wildcard at start.
              // Simple logic: if key == "*" or key ends with suffix matching current state?
              // Actually Deepbound/VS uses "*-none" where "*" matches any prefix.
              // Since we build suffix "-medium-normal", we should check if key matches.

              // Current rudimentary glob match:
              bool match = false;
              if (key == "*")
                match = true;
              else if (key.find("*") != std::string::npos)
              {
                // extremely simple glob: "*-none"
                std::string suffix_part = key.substr(key.find("*") + 1);
                if (current_suffix.length() >= suffix_part.length() && current_suffix.compare(current_suffix.length() - suffix_part.length(), suffix_part.length(), suffix_part) == 0)
                {
                  match = true;
                }
              }

              if (match)
                return it.value();
            }
            return "";
          };

          // Apply drawtypeByType
          if (j.contains("drawtypeByType"))
          {
            std::string dt = check_map_property(j["drawtypeByType"], current_suffix);
            if (!dt.empty())
              var.draw_type = dt;
          }

          // Apply climateColorMapByType
          if (j.contains("climateColorMapByType"))
          {
            // This map might contain nulls, so be careful.
            for (auto it = j["climateColorMapByType"].begin(); it != j["climateColorMapByType"].end(); ++it)
            {
              std::string key = it.key();
              bool match = false;
              if (key == "*")
                match = true;
              else if (key.find("*") != std::string::npos)
              {
                std::string suffix_part = key.substr(key.find("*") + 1);
                if (current_suffix.length() >= suffix_part.length() && current_suffix.compare(current_suffix.length() - suffix_part.length(), suffix_part.length(), suffix_part) == 0)
                {
                  match = true;
                }
              }

              if (match)
              {
                if (!it.value().is_null())
                  var.climate_color_map = it.value();
                else
                  var.climate_color_map = "";
                break; // First match wins logic often better? Or specific? Map iteration order is undefined-ish.
                       // VS likely picks most specific. Here we assume JSON author order or specificity.
              }
            }
          }

          tile_registry_t::get().register_tile(var);
          return;
        }

        // Iterate states of current group
        const auto &g = groups[group_idx];
        for (const auto &state : g.states)
        {
          std::vector<std::pair<std::string, std::string>> next_replacements = replacements;
          next_replacements.push_back({g.code, state});
          generate_combinations(group_idx + 1, current_suffix + "-" + state, next_replacements);
        }
      };

      generate_combinations(0, "", {});
    }
    else
    {
      // ... existing single registration logic ...
      tile_registry_t::get().register_tile(base_def);
    }
  }
  catch (const std::exception &e)
  {
    std::cerr << "Failed to parse tile " << filename << ": " << e.what() << std::endl;
  }
}

// Obsolete worldgen loading removed. WorldGenerator handles its own loading now.

auto json_loader_t::load_color_maps(const std::string &file_path) -> void
{
  std::ifstream file(file_path);
  if (!file.is_open())
  {
    std::cerr << "Failed to open color map config: " << file_path << std::endl;
    return;
  }

  nlohmann::json j;
  file >> j;

  if (!j.is_array())
  {
    std::cerr << "Color map config must be an array." << std::endl;
    return;
  }

  for (const auto &entry : j)
  {
    if (entry.contains("code") && entry.contains("texture"))
    {
      std::string code = entry["code"];
      auto &tex = entry["texture"];
      if (tex.contains("base"))
      {
        std::string base = tex["base"];
        // Construct resource ID: deepbound:textures/environment/plant_tint (AssetManager handles extension)
        resource_id_t id("deepbound", "textures/" + base);

        bool load_atlas = true;
        if (entry.contains("loadIntoBlockTextureAtlas"))
        {
          load_atlas = entry["loadIntoBlockTextureAtlas"];
        }

        asset_manager_t::get().register_color_map(code, id, load_atlas);
      }
    }
  }
}

} // namespace deepbound
