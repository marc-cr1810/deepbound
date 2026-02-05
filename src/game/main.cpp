#include "core/assets/asset_manager.hpp"
#include "core/content/entity.hpp"
#include "core/content/item.hpp"
#include "core/content/tile.hpp"
#include "core/assets/json_loader.hpp"
#include "core/graphics/chunk_renderer.hpp"
#include "core/graphics/window.hpp"
#include "core/graphics/window.hpp"
#include "core/worldgen/world.hpp"
#include <iostream>

// Temporary for glClearColor/glClear without GLEW/GLAD
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

int main(int argc, char *argv[])
{
  std::cout << "Deepbound Game Starting..." << std::endl;

  deepbound::window_t::properties_t props;
  props.title = "Deepbound";
  props.width = 1280;
  props.height = 720;
  props.vsync = true;

  deepbound::window_t window(props);

  // Initialize systems
  auto &asset_mgr = deepbound::asset_manager_t::get();
  asset_mgr.initialize();

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(window.get_native_window(), true);
  ImGui_ImplOpenGL3_Init("#version 330");

  // Load Content
  std::cout << "Loading Content..." << std::endl;
  deepbound::json_loader_t::load_tiles_from_directory("assets/tiles");
  deepbound::json_loader_t::load_color_maps("assets/config/color_maps.json");
  // World generation data is loaded from assets/worldgen/ within the world_generator_t constructor.
  asset_mgr.load_all_textures_from_registry();

  // World Generation
  deepbound::world_t world;

  // Renderer
  deepbound::chunk_renderer_t renderer;
  deepbound::camera_2d_t camera;
  camera.set_position({0.0f, 250.0f}); // Adjusted for new world height/sea level
  camera.set_zoom(0.01f);

  // Setup Zoom Callback
  window.set_scroll_callback([&camera](double x, double y) { camera.zoom_scroll((float)y); });

  float last_time = 0.0f;

  // Main Loop
  while (!window.should_close())
  {
    float current_time = (float)glfwGetTime();
    float delta_time = current_time - last_time;
    last_time = current_time;

    window.update();

    // Input Handling
    float speed = 2.0f * delta_time / camera.get_zoom(); // Adjust speed by zoom
    if (window.is_key_pressed(GLFW_KEY_W))
      camera.move({0.0f, speed});
    if (window.is_key_pressed(GLFW_KEY_S))
      camera.move({0.0f, -speed});
    if (window.is_key_pressed(GLFW_KEY_A))
      camera.move({-speed, 0.0f});
    if (window.is_key_pressed(GLFW_KEY_D))
      camera.move({speed, 0.0f});

    // Update World (Process Async Chunks)
    world.update(delta_time);

    // Render
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Debug Overlay
    {
      double mouse_x, mouse_y;
      glfwGetCursorPos(window.get_native_window(), &mouse_x, &mouse_y);

      // Convert to world space
      // Correct way is: (mouse_ndc * aspect * zoom) + camera_pos
      // Screen Center is (width/2, height/2)
      // 1. Center offset
      float cx = (float)mouse_x - (float)window.get_width() / 2.0f;
      float cy = (float)window.get_height() / 2.0f - (float)mouse_y; // Invert Y

      // 2. Aspect correction (if applied in projection) handled by renderer usually
      // But here we need manual unproject logic similar to camera.
      float zoom = camera.get_zoom();

      // Based on typical 2D orthographic camera:
      // world_pos = camera_pos + (screen_offset / zoom) (ignoring aspect for a moment depending on how projection is set up)
      // Let's assume aspect adjustment is needed if the projection scales width by aspect.
      // Usually: projection = glm::ortho(-w/2 * z, w/2 * z, -h/2 * z, h/2 * z)
      // So world unit = pixel / zoom basically?
      // Let's rely on simple scaling for now.

      // Actually, let's look at camera.zoom logic or just guess iteratively.
      // float world_mouse_x = camera.get_position().x + cx / (window.get_height() / 2.0f) * (1.0f/zoom) * aspect ... complicated.
      // Simplified:
      // World View Height = 2.0 / Zoom (if projection is -1 to 1)
      // OR World View Height = ScreenHeight / Zoom (if pixel perfect)

      // Just guessing based on previous code:
      // float speed = 2.0f * delta_time / camera.get_zoom();

      // Let's try this:
      float aspect = (float)window.get_width() / (float)window.get_height();
      // Assuming height of view is 2.0f / zoom
      float view_h = 2.0f / zoom;
      float view_w = view_h * aspect;

      float relative_x = (float)mouse_x / (float)window.get_width() - 0.5f;  // -0.5 to 0.5
      float relative_y = 0.5f - (float)mouse_y / (float)window.get_height(); // -0.5 to 0.5

      float world_mouse_x = camera.get_position().x + relative_x * view_w;
      float world_mouse_y = camera.get_position().y + relative_y * view_h;

      auto tile_id = world.get_tile_at(world_mouse_x, world_mouse_y);

      ImGui::SetNextWindowPos(ImVec2(window.get_width() * 0.5f, 10.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
      ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
      if (ImGui::Begin("Tile Inspector", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
      {
        if (tile_id)
        {
          ImGui::Text("Tile: %s", tile_id->code.c_str());
          ImGui::Text("Pos: %.1f, %.1f", world_mouse_x, world_mouse_y);
        }
        else
        {
          ImGui::Text("Tile: <None>");
        }
        ImGui::End();
      }
    }

    float aspect = (float)window.get_width() / (float)window.get_height();

    // Render visible chunks
    auto visible_chunks = world.get_visible_chunks(camera.get_position(), 4); // Range 4

    for (auto *chunk : visible_chunks)
    {
      renderer.render(*chunk, camera, aspect);
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    window.swap_buffers();
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  std::cout << "Deepbound Game Shutting Down." << std::endl;
  return 0;
}
