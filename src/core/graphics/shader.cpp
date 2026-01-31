#include "core/graphics/shader.hpp"
#include <iostream>
#include <vector>

namespace deepbound
{

shader_t::shader_t(const std::string &vertex_src, const std::string &fragment_src)
{
  unsigned int vs = compile_shader(GL_VERTEX_SHADER, vertex_src);
  unsigned int fs = compile_shader(GL_FRAGMENT_SHADER, fragment_src);

  m_renderer_id = glCreateProgram();
  glAttachShader(m_renderer_id, vs);
  glAttachShader(m_renderer_id, fs);
  glLinkProgram(m_renderer_id);
  glValidateProgram(m_renderer_id);

  int success;
  glGetProgramiv(m_renderer_id, GL_LINK_STATUS, &success);
  if (!success)
  {
    char infoLog[512];
    glGetProgramInfoLog(m_renderer_id, 512, NULL, infoLog);
    std::cerr << "ERROR: Shader Program Linking Failed\n" << infoLog << std::endl;
  }

  glDeleteShader(vs);
  glDeleteShader(fs);
}

shader_t::~shader_t()
{
  glDeleteProgram(m_renderer_id);
}

auto shader_t::bind() const -> void
{
  glUseProgram(m_renderer_id);
}

auto shader_t::unbind() const -> void
{
  glUseProgram(0);
}

auto shader_t::set_int(const std::string &name, int value) -> void
{
  glUniform1i(get_uniform_location(name), value);
}

auto shader_t::set_float(const std::string &name, float value) -> void
{
  glUniform1f(get_uniform_location(name), value);
}

auto shader_t::compile_shader(unsigned int type, const std::string &source) -> unsigned int
{
  unsigned int id = glCreateShader(type);
  const char *src = source.c_str();
  glShaderSource(id, 1, &src, nullptr);
  glCompileShader(id);

  int result;
  glGetShaderiv(id, GL_COMPILE_STATUS, &result);
  if (result == GL_FALSE)
  {
    int length;
    glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
    std::vector<char> message(length);
    glGetShaderInfoLog(id, length, &length, message.data());
    std::cerr << "Failed to compile " << (type == GL_VERTEX_SHADER ? "vertex" : "fragment") << " shader!" << std::endl;
    std::cerr << message.data() << std::endl;
    glDeleteShader(id);
    return 0;
  }

  return id;
}

auto shader_t::get_uniform_location(const std::string &name) const -> int
{
  if (m_uniform_cache.find(name) != m_uniform_cache.end())
    return m_uniform_cache[name];

  int location = glGetUniformLocation(m_renderer_id, name.c_str());
  if (location == -1)
    std::cerr << "Warning: uniform '" << name << "' doesn't exist!" << std::endl;

  m_uniform_cache[name] = location;
  return location;
}

} // namespace deepbound
