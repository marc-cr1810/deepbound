#include "core/worldgen/world_gen_context.hpp"

namespace deepbound
{

auto world_gen_context_t::get_landform(const std::string &code) -> const landform_variant_t *
{
  for (const auto &lf : landforms)
  {
    if (lf.code == code)
      return &lf;
  }
  return nullptr;
}

} // namespace deepbound
