#include "core/content/tile.hpp"

namespace deepbound {

auto tile_registry_t::get() -> tile_registry_t & {
  static tile_registry_t instance;
  return instance;
}

auto tile_registry_t::register_tile(const tile_definition_t &definition)
    -> void {
  m_tile_map[definition.id] = definition;
}

auto tile_registry_t::get_tile(const resource_id_t &id) const
    -> const tile_definition_t * {
  auto it = m_tile_map.find(id);
  if (it != m_tile_map.end()) {
    return &it->second;
  }
  return nullptr;
}

auto tile_registry_t::get_all_tiles() const
    -> const std::map<resource_id_t, tile_definition_t> & {
  return m_tile_map;
}

} // namespace deepbound
