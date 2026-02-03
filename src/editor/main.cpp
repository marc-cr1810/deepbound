#include "core/assets/asset_manager.hpp"
#include "core/content/entity.hpp"
#include "core/content/item.hpp"
#include "core/content/tile.hpp"
#include <iostream>

int main(int argc, char *argv[])
{
  std::cout << "Deepbound Assets Editor Starting..." << std::endl;

  // Initialize systems
  deepbound::asset_manager_t::get().initialize();

  // The editor can share the same registries!
  auto &tile_registry = deepbound::tile_registry_t::get();
  auto &item_registry = deepbound::item_registry_t::get();
  auto &entity_registry = deepbound::entity_registry_t::get();
  // auto &strata_registry = deepbound::strata_registry_t::get();
  // auto &landform_registry = deepbound::landform_registry_t::get();

  std::cout << "Loaded " << tile_registry.get_all_tiles().size() << " tiles." << std::endl;
  std::cout << "Loaded " << item_registry.get_all_items().size() << " items." << std::endl;
  std::cout << "Loaded " << entity_registry.get_all_entities().size() << " entities." << std::endl;
  // std::cout << "Loaded " << strata_registry.get_all_strata().size() << " strata." << std::endl;
  // std::cout << "Loaded " << landform_registry.get_all_landforms().size() << " landforms." << std::endl;

  // Editor loop placeholder
  bool running = true;
  int frames = 0;
  while (running && frames < 5)
  {
    // ImGui Logic here
    frames++;
  }

  std::cout << "Deepbound Assets Editor Shutting Down." << std::endl;
  return 0;
}
