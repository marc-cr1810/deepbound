#pragma once

#include "core/common/resource_id.hpp"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace deepbound {

/**
 * @brief Represents the data for a single type of tile.
 */
struct tile_drop_t {
  resource_id_t item_id;
  int min_quantity = 1;
  int max_quantity = 1;
  float chance = 1.0f;
};

struct collision_box_t {
  float x1 = 0, y1 = 0, z1 = 0;
  float x2 = 1, y2 = 1, z2 = 1;
};

/**
 * @brief Represents the data for a single type of tile.
 */
struct tile_definition_t {
  resource_id_t id; // The internal numeric/hashed ID or full string resource ID
  std::string code; // e.g. "soil" - the definition name
  std::string class_name; // e.g. "BlockSoil" - C++ class mapping

  std::map<std::string, resource_id_t>
      textures; // e.g. "up" -> "deepbound:soil_top"

  std::vector<std::string> behaviors; // Names of attached behaviors
  std::vector<tile_drop_t> drops;

  std::map<std::string, std::string>
      sounds; // e.g. "break" -> "deepbound:dirt_break"

  // Simple key-value attributes for now, ideally this is a JSON object
  std::map<std::string, std::string> attributes;

  bool is_solid = true;
  float hardness = 1.0f;
  collision_box_t collision_box;
};

/**
 * @brief Registry for all tile definitions.
 */
class tile_registry_t {
public:
  static auto get() -> tile_registry_t &;

  tile_registry_t(const tile_registry_t &) = delete;
  auto operator=(const tile_registry_t &) -> tile_registry_t & = delete;

  auto register_tile(const tile_definition_t &definition) -> void;
  auto get_tile(const resource_id_t &id) const -> const tile_definition_t *;
  auto get_all_tiles() const
      -> const std::map<resource_id_t, tile_definition_t> &;

private:
  tile_registry_t() = default;
  std::map<resource_id_t, tile_definition_t> m_tile_map;
};

} // namespace deepbound
