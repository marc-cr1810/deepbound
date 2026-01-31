#pragma once

#include <FastNoise/FastNoise.h>
#include <memory>
#include <string>
#include <vector>

namespace deepbound
{

class fast_noise_wrapper_t
{
public:
  fast_noise_wrapper_t(int seed = 1337);

  // Re-seed the generator
  auto set_seed(int seed) -> void;

  // Get 2D noise value (-1.0 to 1.0 usually)
  auto get_noise(float x, float y) const -> float;

  // Get Fractal Noise (Simplex/Perlin)
  // frequency: controls scale (lower = larger features)
  // octaves: detail level
  auto get_simplex_fractal(float x, float y, float frequency = 0.01f, int octaves = 3, float lacunarity = 2.0f, float gain = 0.5f) const -> float;

  // Get Cellular/Voronoi Noise (for Provinces)
  auto get_cellular(float x, float y, float frequency = 0.01f) const -> float;

  // Get custom weighted terrain noise with thresholds
  auto get_terrain_noise(float x, float y, const std::vector<double> &amplitudes, const std::vector<double> &thresholds = {}) const -> float;

private:
  int m_seed;
  // We can keep a few preset generators for efficiency
  FastNoise::SmartNode<FastNoise::FractalFBm> m_simplex;
  FastNoise::SmartNode<FastNoise::Simplex> m_simplex_base;
  FastNoise::SmartNode<FastNoise::CellularValue> m_cellular;
};

} // namespace deepbound
