#include "core/graphics/chunk_renderer.hpp"
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include "core/assets/asset_manager.hpp"

namespace deepbound
{

const std::string vertex_shader_src = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;
uniform vec2 uScale = vec2(1.0, 1.0);
uniform vec2 uOffset = vec2(0.0, 0.0);
uniform float uZoom = 1.0;

void main() {
    // Transform position: (Pos - uOffset) * Zoom * AspectScale
    // Camera pos is "world" pos, so moving camera RIGHT means shifting world LEFT.
    // However, aPos here is already local/world unit coordinates (depending on generator).
    // Current aPos is [-1, 1] relative to center?
    // Let's assume uOffset handles the translation.
    
    vec2 pos = (aPos + uOffset) * uZoom;
    gl_Position = vec4(pos * uScale, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

const std::string fragment_shader_src = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D ourTexture;

void main() {
    vec4 texColor = texture(ourTexture, TexCoord);
    if(texColor.a < 0.1)
        discard;
    FragColor = texColor;
}
)";

ChunkRenderer::ChunkRenderer()
{
  // Setup Shader
  m_shader = std::make_unique<Shader>(vertex_shader_src, fragment_shader_src);

  // Setup Geometry placeholders
  glGenVertexArrays(1, &m_vao);
  glGenBuffers(1, &m_vbo);

  glBindVertexArray(m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

  // Vertex attributes: Pos(2), UV(2) = 4 floats
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
}

ChunkRenderer::~ChunkRenderer()
{
  glDeleteVertexArrays(1, &m_vao);
  glDeleteBuffers(1, &m_vbo);
}

auto ChunkRenderer::render(const Chunk &chunk, const Camera2D &camera, float aspect_ratio) -> void
{
  m_shader->bind();

  // Bind Texture Atlas
  glActiveTexture(GL_TEXTURE0);
  auto &atlas = asset_manager_t::get().get_atlas_texture("tiles");
  glBindTexture(GL_TEXTURE_2D, atlas.get_id());

  float scale_x = (aspect_ratio > 1.0f) ? 1.0f / aspect_ratio : 1.0f;
  float scale_y = (aspect_ratio < 1.0f) ? aspect_ratio : 1.0f;

  int locScale = glGetUniformLocation(m_shader->get_renderer_id(), "uScale");
  if (locScale != -1)
    glUniform2f(locScale, scale_x, scale_y);

  int locOffset = glGetUniformLocation(m_shader->get_renderer_id(), "uOffset");
  if (locOffset != -1)
  {
    // Pass negative camera position to simulate camera movement
    glUniform2f(locOffset, -camera.get_position().x, -camera.get_position().y);
  }

  int locZoom = glGetUniformLocation(m_shader->get_renderer_id(), "uZoom");
  if (locZoom != -1)
    glUniform1f(locZoom, camera.get_zoom());

  glBindVertexArray(m_vao);

  std::vector<float> vertices;

  float tile_w = 2.0f / CHUNK_SIZE;
  float tile_h = 2.0f / CHUNK_SIZE;

  for (int y = 0; y < CHUNK_SIZE; ++y)
  {
    for (int x = 0; x < CHUNK_SIZE; ++x)
    {
      auto id = chunk.get_tile(x, y);
      if (id.get_path() == "air")
        continue;

      // Get UVs
      const auto *def = tile_registry_t::get().get_tile(id);
      uv_rect_t uvs = {0, 0, 0, 0};

      if (def && !def->textures.empty())
      {
        // Try "all"
        if (def->textures.contains("all"))
        {
          uvs = asset_manager_t::get().get_texture_uvs("tiles", def->textures.at("all"));
        }
        else
        {
          // Fallback to first
          uvs = asset_manager_t::get().get_texture_uvs("tiles", def->textures.begin()->second);
        }
      }
      else
      {
        // Try looking up by ID directly as fallback?
        uvs = asset_manager_t::get().get_texture_uvs("tiles", id);
      }

      float gx = -1.0f + x * tile_w;
      float gy = -1.0f + y * tile_h;

      // Quad (2 triangles)
      // Triangle 1
      vertices.insert(vertices.end(), {gx, gy, uvs.u1, uvs.v2});                   // Bottom Left
      vertices.insert(vertices.end(), {gx + tile_w, gy, uvs.u2, uvs.v2});          // Bottom Right
      vertices.insert(vertices.end(), {gx + tile_w, gy + tile_h, uvs.u2, uvs.v1}); // Top Right

      // Triangle 2
      vertices.insert(vertices.end(), {gx, gy, uvs.u1, uvs.v2});                   // Bottom Left
      vertices.insert(vertices.end(), {gx + tile_w, gy + tile_h, uvs.u2, uvs.v1}); // Top Right
      vertices.insert(vertices.end(), {gx, gy + tile_h, uvs.u1, uvs.v1});          // Top Left
    }
  }

  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);

  glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 4);
}

} // namespace deepbound
