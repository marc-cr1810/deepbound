#pragma once

#include "core/common/resource_id.hpp"
#include <map>
#include <string>
#include <vector>

namespace deepbound {

/**
 * @brief Represents the data for a single type of entity.
 * Pure data struct.
 */
struct entity_definition_t {
  resource_id_t id;
  std::string class_name; // C++ implementation class to construct

  // Dimensions (World Units)
  float width = 1.0f;
  float height = 1.0f;

  // Base Stats
  float max_health = 10.0f;
  float move_speed = 5.0f;
  float damage = 0.0f;

  // Visuals
  std::string texture_path;
  // Map animation state names to texture regions or sprite resources
  std::map<std::string, std::string> animations;

  // Loot table
  // Item resource ID -> drop chance
  std::map<resource_id_t, float> drops;

  // AI / Behavior tags
  std::vector<std::string> ai_behaviors;

  // Custom Attributes
  std::map<std::string, std::string> attributes;
};

/**
 * @brief Registry for all entity definitions.
 */
class entity_registry_t {
public:
  static auto get() -> entity_registry_t &;

  entity_registry_t(const entity_registry_t &) = delete;
  auto operator=(const entity_registry_t &) -> entity_registry_t & = delete;

  auto register_entity(const entity_definition_t &definition) -> void;
  auto get_entity(const resource_id_t &id) const -> const entity_definition_t *;
  auto get_all_entities() const
      -> const std::map<resource_id_t, entity_definition_t> &;

private:
  entity_registry_t() = default;
  std::map<resource_id_t, entity_definition_t> m_entity_map;
};

} // namespace deepbound
