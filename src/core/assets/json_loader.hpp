#pragma once

#include "core/content/tile.hpp"
#include <string>
#include <vector>

namespace deepbound
{

class json_loader_t
{
public:
  static auto load_tiles_from_directory(const std::string &directory_path) -> void;
  static auto load_strata_from_file(const std::string &file_path) -> void;

private:
  // Now returns void because it registers internally
  static auto parse_and_register_tile(const std::string &json_content, const std::string &filename) -> void;
  static auto parse_strata_json(const std::string &json_content) -> void;
  // Helper functions for parsing specific implementation details would go here
};

} // namespace deepbound
