#include "core/worldgen/fastnoise_wrapper.hpp"

namespace deepbound
{

fast_noise_wrapper_t::fast_noise_wrapper_t(int seed) : m_seed(seed)
{
  // Initialize standard nodes
  // Note: FastNoise2 uses a graph/node system.

  // Simplex Fractal Setup
  auto source = FastNoise::New<FastNoise::Simplex>();
  auto fractal = FastNoise::New<FastNoise::FractalFBm>();
  fractal->SetSource(source);
  m_simplex = fractal;
  m_simplex_base = source;

  // Cellular Setup
  auto cellular = FastNoise::New<FastNoise::CellularValue>();
  cellular->SetDistanceFunction(FastNoise::DistanceFunction::Euclidean);
  cellular->SetValueIndex(0); // Closest cell value
  m_cellular = cellular;
}

auto fast_noise_wrapper_t::set_seed(int seed) -> void
{
  m_seed = seed;
}

auto fast_noise_wrapper_t::get_noise(float x, float y) const -> float
{
  // Placeholder if needed
  return 0.0f;
}

auto fast_noise_wrapper_t::get_simplex_fractal(float x, float y, float frequency, int octaves, float lacunarity, float gain) const -> float
{
  // Configure node
  m_simplex->SetOctaveCount(octaves);
  m_simplex->SetLacunarity(lacunarity);
  m_simplex->SetGain(gain);

  // Scale coordinates
  float nx = x * frequency;
  float ny = y * frequency;

  return m_simplex->GenSingle2D(nx, ny, m_seed);
}

auto fast_noise_wrapper_t::get_cellular(float x, float y, float frequency) const -> float
{
  float nx = x * frequency;
  float ny = y * frequency;
  return m_cellular->GenSingle2D(nx, ny, m_seed);
}

auto fast_noise_wrapper_t::get_terrain_noise(float x, float y, const std::vector<double> &amplitudes, const std::vector<double> &thresholds) const -> float
{
  float total_noise = 0.0f;
  float total_amp = 0.0f;
  float freq = 0.0005f; // Base frequency from VS GenTerra (0.0005)

  for (size_t i = 0; i < amplitudes.size(); ++i)
  {
    float amp = (float)amplitudes[i];
    float th = (i < thresholds.size()) ? (float)thresholds[i] : 0.0f;

    if (amp != 0.0f)
    {
      float val = m_simplex_base->GenSingle2D(x * freq, y * freq, m_seed + (int)i * 1000);
      total_noise += (val - th) * amp;
      total_amp += std::abs(amp);
    }

    freq *= 2.0f;
  }

  return (total_amp > 0.0f) ? (total_noise / total_amp) : 0.0f;
}

} // namespace deepbound
