#include "core/content/item.hpp"
#include <iostream>

namespace deepbound {

auto item_registry_t::get() -> item_registry_t & {
  static item_registry_t instance;
  return instance;
}

auto item_registry_t::register_item(const item_definition_t &definition)
    -> void {
  if (m_item_map.find(definition.id) != m_item_map.end()) {
    std::cerr << "Warning: Overwriting item definition for " << definition.id
              << std::endl;
  }
  m_item_map[definition.id] = definition;
}

auto item_registry_t::get_item(const resource_id_t &id) const
    -> const item_definition_t * {
  auto it = m_item_map.find(id);
  if (it != m_item_map.end()) {
    return &it->second;
  }
  return nullptr;
}

auto item_registry_t::get_all_items() const
    -> const std::map<resource_id_t, item_definition_t> & {
  return m_item_map;
}

} // namespace deepbound
