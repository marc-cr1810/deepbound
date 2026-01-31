#include "core/content/entity.hpp"
#include <iostream>

namespace deepbound {

auto entity_registry_t::get() -> entity_registry_t & {
  static entity_registry_t instance;
  return instance;
}

auto entity_registry_t::register_entity(const entity_definition_t &definition)
    -> void {
  if (m_entity_map.find(definition.id) != m_entity_map.end()) {
    std::cerr << "Warning: Overwriting entity definition for " << definition.id
              << std::endl;
  }
  m_entity_map[definition.id] = definition;
}

auto entity_registry_t::get_entity(const resource_id_t &id) const
    -> const entity_definition_t * {
  auto it = m_entity_map.find(id);
  if (it != m_entity_map.end()) {
    return &it->second;
  }
  return nullptr;
}

auto entity_registry_t::get_all_entities() const
    -> const std::map<resource_id_t, entity_definition_t> & {
  return m_entity_map;
}

} // namespace deepbound
