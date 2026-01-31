#include "core/worldgen/landform.hpp"

namespace deepbound {

auto landform_registry_t::get() -> landform_registry_t & {
  static landform_registry_t instance;
  return instance;
}

auto landform_registry_t::register_landform(
    const landform_definition_t &definition) -> void {
  if (m_landform_map.find(definition.code) != m_landform_map.end()) {
    std::cerr << "Warning: Overwriting landform definition for "
              << definition.code << std::endl;
  }
  m_landform_map[definition.code] = definition;
}

auto landform_registry_t::get_landform(const std::string &code) const
    -> const landform_definition_t * {
  auto it = m_landform_map.find(code);
  if (it != m_landform_map.end()) {
    return &it->second;
  }
  return nullptr;
}

auto landform_registry_t::get_all_landforms() const
    -> const std::map<std::string, landform_definition_t> & {
  return m_landform_map;
}

} // namespace deepbound
