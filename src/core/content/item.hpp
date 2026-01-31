#pragma once

#include "core/common/resource_id.hpp"
#include <map>
#include <string>
#include <vector>

namespace deepbound {

/**
 * @brief Represents the data for a single type of item.
 * Pure data struct, populated from JSON/Files.
 */
struct item_definition_t {
  resource_id_t id;
  std::string name;
  std::string description;

  int max_stack_size = 64;

  // Visuals
  std::string icon_texture_path; // e.g. "items/iron_sword"

  // Data-driven behavior tags
  // e.g. "tool", "consumable", "material"
  std::vector<std::string> tags;

  // Stats (data-driven map for flexibility)
  // e.g. "damage" -> 5.0, "durability" -> 100.0
  std::map<std::string, float> stats;

  // Attributes (for string data)
  // e.g. "rarity" -> "common"
  std::map<std::string, std::string> attributes;
};

/**
 * @brief Registry for all item definitions.
 */
class item_registry_t {
public:
  static auto get() -> item_registry_t &;

  item_registry_t(const item_registry_t &) = delete;
  auto operator=(const item_registry_t &) -> item_registry_t & = delete;

  auto register_item(const item_definition_t &definition) -> void;
  auto get_item(const resource_id_t &id) const -> const item_definition_t *;
  auto get_all_items() const
      -> const std::map<resource_id_t, item_definition_t> &;

private:
  item_registry_t() = default;
  std::map<resource_id_t, item_definition_t> m_item_map;
};

} // namespace deepbound
