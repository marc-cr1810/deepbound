#include "core/worldgen/rock_strata.hpp"
#include <iostream>

namespace deepbound {

auto strata_registry_t::get() -> strata_registry_t & {
  static strata_registry_t instance;
  return instance;
}

auto strata_registry_t::register_strata(const strata_definition_t &definition)
    -> void {
  if (m_strata_map.find(definition.block_id) != m_strata_map.end()) {
    std::cerr << "Warning: Overwriting strata definition for "
              << definition.block_id << std::endl;
  }
  m_strata_map[definition.block_id] = definition;
}

auto strata_registry_t::get_strata(const resource_id_t &block_id) const
    -> const strata_definition_t * {
  auto it = m_strata_map.find(block_id);
  if (it != m_strata_map.end()) {
    return &it->second;
  }
  return nullptr;
}

auto strata_registry_t::get_all_strata() const
    -> const std::map<resource_id_t, strata_definition_t> & {
  return m_strata_map;
}

} // namespace deepbound
