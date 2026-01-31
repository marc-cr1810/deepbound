#pragma once

#include "core/common/resource_id.hpp"
#include <map>
#include <string>
#include <vector>

namespace deepbound
{

struct uv_rect_t
{
  float u1, v1, u2, v2;
};

class texture_t
{
public:
  texture_t() = default;
  ~texture_t();

  auto load_from_file(const std::string &path) -> bool;
  auto bind(unsigned int unit = 0) const -> void;

  auto get_width() const -> int
  {
    return m_width;
  }
  auto get_height() const -> int
  {
    return m_height;
  }
  auto get_id() const -> unsigned int
  {
    return m_id;
  }

  friend class texture_atlas_t;

private:
  unsigned int m_id = 0;
  int m_width = 0;
  int m_height = 0;
};

class texture_atlas_t
{
public:
  texture_atlas_t(int width, int height);
  ~texture_atlas_t() = default;

  // Adds a texture to the atlas and returns its UVs
  auto add_texture(const resource_id_t &id, const std::string &file_path) -> bool;

  // Gets UVs for a registered texture
  auto get_uvs(const resource_id_t &id) const -> uv_rect_t;

  auto get_texture() const -> const texture_t &
  {
    return m_texture;
  }

private:
  texture_t m_texture;
  std::map<resource_id_t, uv_rect_t> m_uv_map;
  int m_current_x = 0;
  int m_current_y = 0;
  int m_row_height = 0;
  int m_width;
  int m_height;
};

} // namespace deepbound
