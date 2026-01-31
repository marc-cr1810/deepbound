#include "core/assets/json_loader.hpp"
#include "core/common/resource_id.hpp"
#include "core/content/tile.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace deepbound
{

using json = nlohmann::json;

auto json_loader_t::load_tiles_from_directory(const std::string &directory_path) -> void
{
  std::cout << "Loading tiles from: " << directory_path << std::endl;

  if (!std::filesystem::exists(directory_path))
  {
    // Just a warning, not finding the folder is fine if it's not created yet
    std::cout << "Directory does not exist (skipping): " << directory_path << std::endl;
    return;
  }

  for (const auto &entry : std::filesystem::recursive_directory_iterator(directory_path))
  {
    if (entry.is_regular_file() && entry.path().extension() == ".json")
    {
      std::ifstream file(entry.path());
      if (!file.is_open())
        continue;

      try
      {
        json j;
        file >> j;

        parse_and_register_tile(j.dump(), entry.path().filename().string());
      }
      catch (const std::exception &e)
      {
        std::cerr << "Failed to parse " << entry.path() << ": " << e.what() << std::endl;
      }
    }
  }
}

// Helper for string replacement
static std::string replace_placeholders(std::string text, const std::map<std::string, std::string> &replacements)
{
  for (const auto &[key, value] : replacements)
  {
    std::string placeholder = "{" + key + "}";
    size_t pos = 0;
    while ((pos = text.find(placeholder, pos)) != std::string::npos)
    {
      text.replace(pos, placeholder.length(), value);
      pos += value.length();
    }
  }
  return text;
}

// Helper to recursively generate variants
// This is a simplified version of what VS does.
// current_code: "rock"
// group_index: 0
// groups: [ {code:"type", states:["granite", ...]} ]
// accumulated_suffix: ""
static void recurse_variants(const std::string &base_code, int group_index, const std::vector<nlohmann::json> &groups, std::string current_suffix, std::map<std::string, std::string> replacements, const nlohmann::json &root_json,
                             const std::string &filename)
{

  if (group_index >= groups.size())
  {
    // Base case: Register this combination
    tile_definition_t def;

    // Construct final code
    std::string final_code = base_code;
    if (!current_suffix.empty())
    {
      final_code += current_suffix;
    }

    def.code = final_code;
    def.id = resource_id_t("deepbound", final_code);

    if (root_json.contains("class"))
    {
      def.class_name = root_json["class"].get<std::string>();
    }
    else
    {
      def.class_name = "BlockGeneric";
    }

    // Attributes
    if (root_json.contains("attributes"))
    {
      auto &attr = root_json["attributes"];
      if (attr.contains("sidesolid"))
      {
        auto &ss = attr["sidesolid"];
        if (ss.contains("all"))
          def.is_solid = ss["all"].get<bool>();
      }

      // Use mapColorCode if present
      if (attr.contains("mapColorCode"))
      {
        // For now we don't store color in definition but we could
      }
    }

    // TEXTURES
    if (root_json.contains("textures"))
    {
      auto &texts = root_json["textures"];
      for (auto it = texts.begin(); it != texts.end(); ++it)
      {
        std::string key = it.key();
        std::string val = it.value().get<std::string>();

        // Resolve placeholders
        std::string resolved = replace_placeholders(val, replacements);

        // Store with "deepbound" domain default
        def.textures[key] = resource_id_t("deepbound", resolved);
      }
    }

    tile_registry_t::get().register_tile(def);
    // std::cout << "Registered tile variant: " << def.code << std::endl;
    return;
  }

  // Recursive step
  const auto &group = groups[group_index];
  std::string group_code = group.value("code", "");

  if (group.contains("states"))
  {
    for (const auto &state : group["states"])
    {
      std::string state_str = state.get<std::string>();

      std::string next_suffix = current_suffix + "-" + state_str;

      // Update replacements
      auto next_replacements = replacements;
      if (!group_code.empty())
      {
        next_replacements[group_code] = state_str;
      }

      recurse_variants(base_code, group_index + 1, groups, next_suffix, next_replacements, root_json, filename);
    }
  }
}

auto json_loader_t::parse_and_register_tile(const std::string &json_content, const std::string &filename) -> void
{
  auto j = json::parse(json_content);

  std::string code = filename;
  if (code.find(".json") != std::string::npos)
    code = code.substr(0, code.find(".json"));

  if (j.contains("code"))
  {
    code = j["code"].get<std::string>();
  }

  std::map<std::string, std::string> replacements;

  // Check for variants
  if (j.contains("variantgroups"))
  {
    std::vector<json> groups;
    for (const auto &g : j["variantgroups"])
    {
      groups.push_back(g);
    }
    recurse_variants(code, 0, groups, "", replacements, j, filename);
  }
  else
  {
    // No variants, register single
    std::vector<json> empty_groups;
    recurse_variants(code, 0, empty_groups, "", replacements, j, filename);
  }
}

} // namespace deepbound

#include "core/worldgen/rock_strata.hpp"

namespace deepbound
{

auto json_loader_t::load_strata_from_file(const std::string &file_path) -> void
{
  std::cout << "Loading strata from: " << file_path << std::endl;
  std::ifstream file(file_path);
  if (!file.is_open())
  {
    std::cerr << "Failed to open strata file: " << file_path << std::endl;
    return;
  }

  try
  {
    json j;
    file >> j;
    parse_strata_json(j.dump());
  }
  catch (const std::exception &e)
  {
    std::cerr << "Failed to parse strata: " << e.what() << std::endl;
  }
}

auto json_loader_t::parse_strata_json(const std::string &json_content) -> void
{
  auto j = json::parse(json_content);

  if (j.contains("variants") && j["variants"].is_array())
  {
    for (const auto &variant : j["variants"])
    {
      strata_definition_t def;

      std::string blockcode = variant["blockcode"].get<std::string>();
      def.block_id = resource_id_t("deepbound", blockcode);

      if (variant.contains("amplitudes"))
        def.amplitudes = variant["amplitudes"].get<std::vector<float>>();
      if (variant.contains("thresholds"))
        def.thresholds = variant["thresholds"].get<std::vector<float>>();
      if (variant.contains("frequencies"))
        def.frequencies = variant["frequencies"].get<std::vector<float>>();

      std::string dir = variant.value("genDir", "BottomUp");
      if (dir == "TopDown")
        def.gen_dir = strata_gen_dir_t::TopDown;
      else
        def.gen_dir = strata_gen_dir_t::BottomUp;

      // Register
      strata_registry_t::get().register_strata(def);
      // std::cout << "Registered strata: " << blockcode << std::endl;
    }
  }
}

} // namespace deepbound
