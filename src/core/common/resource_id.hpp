#pragma once

#include <string>
#include <string_view>
#include <ostream>

namespace deepbound
{

class resource_id_t
{
public:
  resource_id_t() = default;
  resource_id_t(const std::string& id_string);
  resource_id_t(std::string_view namespace_name, std::string_view path);

  auto get_namespace() const -> const std::string&;
  auto get_path() const -> const std::string&;
  auto to_string() const -> std::string;

  auto operator==(const resource_id_t& other) const -> bool;
  auto operator!=(const resource_id_t& other) const -> bool;
  auto operator<(const resource_id_t& other) const -> bool;

  friend auto operator<<(std::ostream& os, const resource_id_t& id) -> std::ostream&;

private:
  std::string m_namespace = "deepbound";
  std::string m_path;
};

} // namespace deepbound
