#pragma once

#include "core/common/resource_id.hpp"
#include <map>
#include <string>
#include <vector>

namespace deepbound {

enum class strata_gen_dir_t { TopDown, BottomUp };

enum class rock_group_t {
  Sedimentary,
  Metamorphic,
  Igneous,
  Volcanic,
  Unknown
};

/**
 * @brief Represents a geological rock layer definition.
 * Based on Vintage Story's rockstrata.json
 */
struct strata_definition_t {
  resource_id_t block_id; // The block to place (e.g. "rock-granite")

  // Generation parameters (Noise)
  std::vector<float> amplitudes;
  std::vector<float> thresholds;
  std::vector<float> frequencies;

  strata_gen_dir_t gen_dir = strata_gen_dir_t::BottomUp;
  rock_group_t rock_group = rock_group_t::Unknown;
};

/**
 * @brief Registry for all rock strata definitions.
 */
class strata_registry_t {
public:
  static auto get() -> strata_registry_t &;

  strata_registry_t(const strata_registry_t &) = delete;
  auto operator=(const strata_registry_t &) -> strata_registry_t & = delete;

  auto register_strata(const strata_definition_t &definition) -> void;
  // Strata are usually iterated over or looked up by block ID,
  // but we might want them ordered. For now, map by block ID.
  auto get_strata(const resource_id_t &block_id) const
      -> const strata_definition_t *;
  auto get_all_strata() const
      -> const std::map<resource_id_t, strata_definition_t> &;

private:
  strata_registry_t() = default;
  std::map<resource_id_t, strata_definition_t> m_strata_map;
};

} // namespace deepbound
