#include "core/assets/texture_atlas.hpp"
#include <iostream>
#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace deepbound
{

texture_t::~texture_t()
{
  if (m_id != 0)
  {
    glDeleteTextures(1, &m_id);
  }
}

// Loads a single texture from disk (not used by atlas directly usually)
auto texture_t::load_from_file(const std::string &path) -> bool
{
  int width, height, nrChannels;
  // stbi_set_flip_vertically_on_load(true); // Minecraft/block textures usually not flipped
  unsigned char *data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);

  if (data)
  {
    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);

    GLenum format = GL_RGB;
    if (nrChannels == 4)
      format = GL_RGBA;

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // Pixel art look:
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    m_width = width;
    m_height = height;

    stbi_image_free(data);
    return true;
  }
  else
  {
    std::cerr << "Failed to load texture: " << path << std::endl;
    return false;
  }
}

auto texture_t::bind(unsigned int unit) const -> void
{
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, m_id);
}

// -----------------------------------------------------------------------------

texture_atlas_t::texture_atlas_t(int width, int height) : m_width(width), m_height(height)
{

  // Initialize the main texture
  glGenTextures(1, &m_texture.m_id);
  glBindTexture(GL_TEXTURE_2D, m_texture.m_id);

  // Allocate empty storage
  // Use RGBA8
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

  // Nearest neighbor for pixel art
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // No mipmaps for atlas for now
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  m_texture.m_width = width;
  m_texture.m_height = height;

  // Setup initial cursor
  m_current_x = 0;
  m_current_y = 0;
  m_row_height = 0;
}

auto texture_atlas_t::add_texture(const resource_id_t &id, const std::string &file_path) -> bool
{
  int width, height, nrChannels;
  unsigned char *data = stbi_load(file_path.c_str(), &width, &height, &nrChannels, 4); // Force 4 channels (RGBA)

  if (!data)
  {
    std::cerr << "Failed to load texture for atlas: " << file_path << std::endl;
    return false;
  }

  // Simple packing: fill row, move to next
  if (m_current_x + width > m_width)
  {
    m_current_x = 0;
    m_current_y += m_row_height;
    m_row_height = 0;
  }

  if (m_current_y + height > m_height)
  {
    std::cerr << "Texture atlas full! Cannot add " << file_path << std::endl;
    stbi_image_free(data);
    return false;
  }

  // Update row height
  if (height > m_row_height)
    m_row_height = height;

  // Upload sub-image
  glBindTexture(GL_TEXTURE_2D, m_texture.m_id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, m_current_x, m_current_y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);

  // Calculate UVs
  // UV coordinates are normalized [0, 1]
  // Beware of pixel centers vs edges, but for nearest neighbor simple division is usually okay
  float u1 = (float)m_current_x / m_width;
  float v1 = (float)m_current_y / m_height;
  float u2 = (float)(m_current_x + width) / m_width;
  float v2 = (float)(m_current_y + height) / m_height;

  m_uv_map[id] = {u1, v1, u2, v2};

  // Advance cursor
  m_current_x += width;

  stbi_image_free(data);
  return true;
}

auto texture_atlas_t::get_uvs(const resource_id_t &id) const -> uv_rect_t
{
  auto it = m_uv_map.find(id);
  if (it != m_uv_map.end())
  {
    return it->second;
  }
  return {0.0f, 0.0f, 0.0f, 0.0f}; // Error UVs
}

} // namespace deepbound
