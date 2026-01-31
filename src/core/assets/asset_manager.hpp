#pragma once

#include "core/assets/json_loader.hpp"
#include "core/assets/texture_atlas.hpp"
#include "core/common/resource_id.hpp"
#include <map>
#include <memory>
#include <string>

namespace deepbound
{

/**
 * @brief Manages all game assets, including multiple texture atlases.
 */
class asset_manager_t
{
public:
  static auto get() -> asset_manager_t &;

  asset_manager_t(const asset_manager_t &) = delete;
  auto operator=(const asset_manager_t &) -> asset_manager_t & = delete;

  // Initializes the fallback texture and default atlases
  auto initialize() -> void;

  // Registers a texture to a specific atlas
  auto register_texture(const std::string &atlas_name, const resource_id_t &id, const std::string &file_path) -> bool;

  // Returns the UVs for a texture, falling back to "deepbound:unknown" if not
  // found
  auto get_texture_uvs(const std::string &atlas_name, const resource_id_t &id) -> uv_rect_t;

  // Gets the texture object for an atlas
  auto get_atlas_texture(const std::string &atlas_name) -> const texture_t &;

  // Loads all textures referenced by registered content
  auto load_all_textures_from_registry() -> void;

private:
  asset_manager_t() = default;

  std::map<std::string, std::unique_ptr<texture_atlas_t>> m_atlases;
  resource_id_t m_fallback_id = resource_id_t("deepbound:unknown");
};

} // namespace deepbound
