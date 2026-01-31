#include "core/assets/asset_manager.hpp"
#include "core/content/entity.hpp"
#include "core/content/item.hpp"
#include "core/content/tile.hpp"
#include "core/assets/json_loader.hpp"
#include "core/graphics/chunk_renderer.hpp"
#include "core/graphics/window.hpp"
#include "core/worldgen/landform.hpp"
#include "core/worldgen/rock_strata.hpp"
#include "core/graphics/window.hpp"
#include "core/worldgen/world_generator.hpp"
#include "core/worldgen/world.hpp"
#include <iostream>

// Temporary for glClearColor/glClear without GLEW/GLAD
#include <GLFW/glfw3.h>

int main(int argc, char *argv[])
{
  std::cout << "Deepbound Game Starting..." << std::endl;

  deepbound::Window::Properties props;
  props.title = "Deepbound";
  props.width = 1280;
  props.height = 720;
  props.vsync = true;

  deepbound::Window window(props);

  // Initialize systems
  auto &asset_mgr = deepbound::asset_manager_t::get();
  asset_mgr.initialize();

  // Load Content
  std::cout << "Loading Content..." << std::endl;
  deepbound::json_loader_t::load_tiles_from_directory("assets/tiles");
  // World generation data is loaded from assets/worldgen/ within the world_generator_t constructor.
  asset_mgr.load_all_textures_from_registry();

  // World Generation
  deepbound::World world;

  // Renderer
  deepbound::ChunkRenderer renderer;
  deepbound::Camera2D camera;
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

    // Render
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    float aspect = (float)window.get_width() / (float)window.get_height();

    // Render visible chunks
    auto visible_chunks = world.get_visible_chunks(camera.get_position(), 4); // Range 4
    for (auto *chunk : visible_chunks)
    {
      renderer.render(*chunk, camera, aspect);
    }

    window.swap_buffers();
  }

  std::cout << "Deepbound Game Shutting Down." << std::endl;
  return 0;
}
