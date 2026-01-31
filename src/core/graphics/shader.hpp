#pragma once

#include <string>
#include <unordered_map>

#include <GLFW/glfw3.h>
#include <glad/glad.h>

namespace deepbound
{

class Shader
{
public:
  Shader(const std::string &vertex_src, const std::string &fragment_src);
  ~Shader();

  auto bind() const -> void;
  auto unbind() const -> void;
  auto get_renderer_id() const -> unsigned int
  {
    return m_renderer_id;
  }

  auto set_int(const std::string &name, int value) -> void;
  auto set_float(const std::string &name, float value) -> void;
  // Matrix setters would go here (need glm)

private:
  auto compile_shader(unsigned int type, const std::string &source) -> unsigned int;
  auto get_uniform_location(const std::string &name) const -> int;

  unsigned int m_renderer_id;
  mutable std::unordered_map<std::string, int> m_uniform_cache;
};

} // namespace deepbound
