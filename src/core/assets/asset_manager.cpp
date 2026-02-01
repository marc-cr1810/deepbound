#include "core/assets/asset_manager.hpp"
#include "core/content/tile.hpp"
#include <iostream>

namespace deepbound
{

auto asset_manager_t::get() -> asset_manager_t &
{
  static asset_manager_t instance;
  return instance;
}

auto asset_manager_t::initialize() -> void
{
  // Always create at least the "tiles" and "items" atlases
  m_atlases["tiles"] = std::make_unique<texture_atlas_t>(2048, 2048);
  m_atlases["items"] = std::make_unique<texture_atlas_t>(2048, 2048);

  // Register the fallback texture
  // Assuming the user's path provided: assets/textures/unknown.png
  register_texture("tiles", m_fallback_id, "assets/textures/unknown.png");
}

auto asset_manager_t::register_texture(const std::string &atlas_name, const resource_id_t &id, const std::string &file_path) -> bool
{
  auto it = m_atlases.find(atlas_name);
  if (it == m_atlases.end())
  {
    std::cerr << "Atlas not found: " << atlas_name << std::endl;
    return false;
  }

  return it->second->add_texture(id, file_path);
}

auto asset_manager_t::get_texture_uvs(const std::string &atlas_name, const resource_id_t &id) -> uv_rect_t
{
  auto it = m_atlases.find(atlas_name);
  if (it == m_atlases.end())
  {
    return {0, 0, 0, 0};
  }

  uv_rect_t uvs = it->second->get_uvs(id);

  // Checking for "missing" UV (stub implementation returns 0s for now)
  if (uvs.u1 == 0 && uvs.v1 == 0 && uvs.u2 == 0 && uvs.v2 == 0)
  {
    return it->second->get_uvs(m_fallback_id);
  }

  return uvs;
}

auto asset_manager_t::get_atlas_texture(const std::string &atlas_name) -> const texture_t &
{
  return m_atlases.at(atlas_name)->get_texture();
}

auto asset_manager_t::load_all_textures_from_registry() -> void
{
  // Access registries
  const auto &tiles = tile_registry_t::get().get_all_tiles();

  for (const auto &[id, def] : tiles)
  {
    if (def.textures.empty())
      continue;

    for (const auto &[key, tex_id] : def.textures)
    {
      // Construct file path
      // Constructed path: assets/textures/path.png (Flattened structure)
      std::string path = "assets/textures/" + tex_id.get_path() + ".png";
      register_texture("tiles", tex_id, path);
    }

    // Load special second texture if present
    if (!def.special_second_texture.get_path().empty() && def.special_second_texture.get_path() != "deepbound:unknown")
    {
      std::string path = "assets/textures/" + def.special_second_texture.get_path() + ".png";
      register_texture("tiles", def.special_second_texture, path);
    }
  }

  // Load Tint Maps into Atlas if requested
  for (const auto &[code, info] : m_color_maps)
  {
    if (info.load_into_atlas)
    {
      std::string path = "assets/" + info.id.get_path() + ".png";
      register_texture("tiles", info.id, path);
    }
  }
}

auto asset_manager_t::get_texture(const resource_id_t &id) -> const texture_t &
{
  auto it = m_standalone_textures.find(id);
  if (it != m_standalone_textures.end())
  {
    return *it->second;
  }

  // Attempt to load
  std::string path = "assets/" + id.get_path();

  try
  {
    auto new_tex = std::make_unique<texture_t>();
    if (!new_tex->load_from_file(path))
    {
      // Try appending .png
      std::string path_png = path + ".png";
      if (!new_tex->load_from_file(path_png))
      {
        throw std::runtime_error("Failed to load texture at " + path + " or " + path_png);
      }
    }
    auto &ref = *new_tex;
    m_standalone_textures[id] = std::move(new_tex);
    return ref;
  }
  catch (const std::exception &e)
  {
    std::cerr << "Truly failed to load texture " << id.get_path() << ": " << e.what() << std::endl;
    throw;
  }
}

auto asset_manager_t::register_color_map(const std::string &code, const resource_id_t &texture_id, bool load_into_atlas) -> void
{
  m_color_maps[code] = {texture_id, load_into_atlas};
}

auto asset_manager_t::get_color_map_texture_id(const std::string &code) -> resource_id_t
{
  if (m_color_maps.count(code))
  {
    return m_color_maps.at(code).id;
  }
  return resource_id_t("deepbound:missing_color_map");
}

} // namespace deepbound
