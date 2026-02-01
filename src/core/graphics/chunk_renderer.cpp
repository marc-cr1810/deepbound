#include "core/graphics/chunk_renderer.hpp"
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include "core/assets/asset_manager.hpp"
#include "core/worldgen/chunk.hpp"
#include "core/content/tile.hpp"

namespace deepbound
{

const std::string vertex_shader_src = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec2 aClimate; // x=Temp, y=Rain
layout (location = 3) in float aTintId; // 0=None, 1=Plant, 2=Water, etc.

out vec2 TexCoord;
out vec2 vClimate;
out float vTintId;

uniform vec2 uScale = vec2(1.0, 1.0);
uniform vec2 uOffset = vec2(0.0, 0.0);
uniform float uZoom = 1.0;

void main() {
    vec2 pos = (aPos - uOffset) * uZoom;
    gl_Position = vec4(pos * uScale, 0.0, 1.0);
    TexCoord = aTexCoord;
    vClimate = aClimate;
    vTintId = aTintId;
}
)";

const std::string fragment_shader_src = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec2 vClimate;
in float vTintId;

uniform sampler2D uAtlas;     // Base tiles (Slot 0)
// UV Bounds for tint maps in the atlas (u1, v1, u2, v2)
uniform vec4 uTintUVs[8]; 

void main() {
    vec4 texColor = texture(uAtlas, TexCoord);
    if(texColor.a < 0.1)
        discard;
    
    // Tint Logic
    vec4 tint = vec4(1.0);
    int id = int(vTintId + 0.5); // Round to nearest int
    
    if (id > 0) {
        vec2 tintUV = clamp(vClimate, 0.0, 1.0);
        
        int idx = id - 1; 
        vec4 bounds = vec4(0.0);
        
        switch(idx) {
            case 0: bounds = uTintUVs[0]; break;
            case 1: bounds = uTintUVs[1]; break;
            case 2: bounds = uTintUVs[2]; break;
            case 3: bounds = uTintUVs[3]; break;
            case 4: bounds = uTintUVs[4]; break;
            case 5: bounds = uTintUVs[5]; break;
            case 6: bounds = uTintUVs[6]; break;
            case 7: bounds = uTintUVs[7]; break;
        }
        
        // Sample from Atlas using remapped UVs
        // bounds = (u1, v1, u2, v2)
        // We act as if tintUV (0..1) maps to (u1,v1)..(u2,v2)
        // Note: Y coordinates might need check depending on atlas packing.
        // Assuming standard:
        vec2 finalUV;
        finalUV.x = mix(bounds.x, bounds.z, tintUV.x);
        finalUV.y = mix(bounds.y, bounds.w, tintUV.y);
        
        // If bounds are valid (not zero), sample.
        if (bounds.x != bounds.z) {
             tint = texture(uAtlas, finalUV);
        }
    }
    
    FragColor = texColor * tint;
}
)";

chunk_renderer_t::chunk_renderer_t()
{
  // Setup Shader
  m_shader = std::make_unique<shader_t>(vertex_shader_src, fragment_shader_src);

  // Setup Geometry placeholders
  glGenVertexArrays(1, &m_vao);
  glGenBuffers(1, &m_vbo);

  glBindVertexArray(m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

  // Vertex attributes: Pos(2), UV(2), Climate(2), TintId(1) = 7 floats
  int stride = 7 * sizeof(float);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void *)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)(2 * sizeof(float)));

  // Attribute 2: Climate (vec2)
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void *)(4 * sizeof(float)));

  // Attribute 3: TintId (float)
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void *)(6 * sizeof(float)));
}

chunk_renderer_t::~chunk_renderer_t()
{
  glDeleteBuffers(1, &m_vbo);
  glDeleteVertexArrays(1, &m_vao);
}

auto chunk_renderer_t::render(const chunk_t &chunk, const camera_2d_t &camera, float aspect_ratio) -> void
{
  m_shader->bind();

  // Bind Texture Atlas (Slot 0)
  glActiveTexture(GL_TEXTURE0);
  auto &atlas = asset_manager_t::get().get_atlas_texture("tiles");
  glBindTexture(GL_TEXTURE_2D, atlas.get_id());
  glUniform1i(glGetUniformLocation(m_shader->get_renderer_id(), "uAtlas"), 0);

  // Bind Tint UVs (lookup from Atlas)
  std::map<std::string, int> tint_slots;
  std::vector<float> tint_uv_array; // Flattened vec4 array
  auto &color_maps = asset_manager_t::get().get_color_maps();

  int current_slot = 1; // 1-based index for shader logic
  for (const auto &[code, info] : color_maps)
  {
    if (current_slot > 8)
      break;

    uv_rect_t uvs = {0, 0, 0, 0};

    if (info.load_into_atlas)
    {
      uvs = asset_manager_t::get().get_texture_uvs("tiles", info.id);
    }
    else
    {
      // Fallback or ignore standalone for this implementation
      // If we supported mixed, we'd need uTints and uTintUVs mixed usage.
      // For now, assume Atlas.
    }

    tint_slots[code] = current_slot;

    // Push u1, v1, u2, v2
    tint_uv_array.push_back(uvs.u1);
    tint_uv_array.push_back(uvs.v1);
    tint_uv_array.push_back(uvs.u2);
    tint_uv_array.push_back(uvs.v2);

    current_slot++;
  }

  // Pad with zeroes
  while (tint_uv_array.size() < 8 * 4)
  {
    tint_uv_array.push_back(0.0f);
  }

  // Set Uniform Array
  int locTintUVs = glGetUniformLocation(m_shader->get_renderer_id(), "uTintUVs");
  if (locTintUVs != -1)
  {
    // Pass raw float array, 8 vec4s = 32 floats
    glUniform4fv(locTintUVs, 8, tint_uv_array.data());
  }

  float scale_x = (aspect_ratio > 1.0f) ? 1.0f / aspect_ratio : 1.0f;
  float scale_y = (aspect_ratio < 1.0f) ? aspect_ratio : 1.0f;

  int locScale = glGetUniformLocation(m_shader->get_renderer_id(), "uScale");
  if (locScale != -1)
    glUniform2f(locScale, scale_x, scale_y);

  int locOffset = glGetUniformLocation(m_shader->get_renderer_id(), "uOffset");
  if (locOffset != -1)
  {
    glUniform2f(locOffset, camera.get_position().x, camera.get_position().y);
  }

  int locZoom = glGetUniformLocation(m_shader->get_renderer_id(), "uZoom");
  if (locZoom != -1)
    glUniform1f(locZoom, camera.get_zoom());

  glBindVertexArray(m_vao);

  if (chunk.is_mesh_dirty())
  {
    std::vector<float> vertices;
    vertices.reserve(CHUNK_SIZE * CHUNK_SIZE * 42); // Reserved 7 floats * 6 verts * blocks

    float chunk_world_x = (float)chunk.get_x() * CHUNK_SIZE;
    float chunk_world_y = (float)chunk.get_y() * CHUNK_SIZE;

    for (int y = 0; y < CHUNK_SIZE; ++y)
    {
      for (int x = 0; x < CHUNK_SIZE; ++x)
      {
        auto &id = chunk.get_tile(x, y);
        if (id.get_path() == "air")
          continue;

        const auto *def = tile_registry_t::get().get_tile(id);

        // Climate Data
        auto clim = chunk.get_climate(x, y);
        // Normalize: Temp -50..50 -> 0..1 (approx)
        float n_temp = (clim.temp + 50.0f) / 100.0f;
        n_temp = std::clamp(n_temp, 0.0f, 1.0f);
        // Rain 0..255 -> 0..1
        float n_rain = clim.rain / 255.0f;
        n_rain = std::clamp(n_rain, 0.0f, 1.0f);

        // Tint ID lookup
        float block_tint_id = 0.0f;
        if (def && !def->climate_color_map.empty())
        {
          // Lookup in our slot map (data driven)
          if (tint_slots.count(def->climate_color_map))
          {
            // If mapped to slot 1, shader logic expects ID?
            // Shader: idx = id - 1. if id=1, idx=0 -> uTints[0] -> Slot 1. Correct.
            block_tint_id = (float)tint_slots[def->climate_color_map];
          }
        }

        uv_rect_t uvs = {0, 0, 0, 0};
        if (def && !def->textures.empty())
        {
          if (def->textures.contains("all"))
            uvs = asset_manager_t::get().get_texture_uvs("tiles", def->textures.at("all"));
          else
            uvs = asset_manager_t::get().get_texture_uvs("tiles", def->textures.begin()->second);
        }
        else
        {
          uvs = asset_manager_t::get().get_texture_uvs("tiles", id);
        }

        float gx = chunk_world_x + (float)x;
        float gy = chunk_world_y + (float)y;

        // Helper to push a quad
        auto push_quad = [&](const uv_rect_t &uv, float t_id)
        {
          vertices.insert(vertices.end(), {gx, gy, uv.u1, uv.v2, n_temp, n_rain, t_id, gx + 1.0f, gy,        uv.u2, uv.v2, n_temp, n_rain, t_id, gx + 1.0f, gy + 1.0f, uv.u2, uv.v1, n_temp, n_rain, t_id,
                                           gx, gy, uv.u1, uv.v2, n_temp, n_rain, t_id, gx + 1.0f, gy + 1.0f, uv.u2, uv.v1, n_temp, n_rain, t_id, gx,        gy + 1.0f, uv.u1, uv.v1, n_temp, n_rain, t_id});
        };

        if (def && def->draw_type == "TopSoil" && !def->special_second_texture.get_path().empty())
        {
          // TopSoil Logic:
          // 1. Base Texture (Untinted)
          uv_rect_t base_uv = {0, 0, 0, 0};
          if (!def->textures.empty())
          {
            if (def->textures.contains("all"))
              base_uv = asset_manager_t::get().get_texture_uvs("tiles", def->textures.at("all"));
            else
              base_uv = asset_manager_t::get().get_texture_uvs("tiles", def->textures.begin()->second);
          }
          else
          {
            base_uv = asset_manager_t::get().get_texture_uvs("tiles", id);
          }
          push_quad(base_uv, 0.0f);

          // 2. Special Second Texture (Tinted) - "Side" overlay
          uv_rect_t overlay_uv = asset_manager_t::get().get_texture_uvs("tiles", def->special_second_texture);
          if (overlay_uv.u1 != overlay_uv.u2)
          {
            push_quad(overlay_uv, block_tint_id);
          }
        }
        else
        {
          // Standard Logic
          // 1. Draw Base
          float base_tint = (def && !def->overlays.empty()) ? 0.0f : block_tint_id;
          push_quad(uvs, base_tint);

          // 2. Draw Overlays
          if (def && !def->overlays.empty())
          {
            for (const auto &overlay_id : def->overlays)
            {
              uv_rect_t overlay_uv = asset_manager_t::get().get_texture_uvs("tiles", overlay_id);
              if (overlay_uv.u1 != overlay_uv.u2)
              {
                push_quad(overlay_uv, block_tint_id);
              }
            }
          }
        }
      }
    }
    chunk.set_mesh(std::move(vertices));
  }

  const auto &mesh = chunk.get_mesh();
  if (mesh.empty())
    return;

  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBufferData(GL_ARRAY_BUFFER, mesh.size() * sizeof(float), mesh.data(), GL_STATIC_DRAW);

  // 7 floats per vertex now
  glDrawArrays(GL_TRIANGLES, 0, mesh.size() / 7);
}

} // namespace deepbound
