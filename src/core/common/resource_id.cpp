#include "core/common/resource_id.hpp"
#include <sstream>

namespace deepbound {

resource_id_t::resource_id_t(const std::string &id_string) {
  size_t colon_pos = id_string.find(':');
  if (colon_pos != std::string::npos) {
    m_namespace = id_string.substr(0, colon_pos);
    m_path = id_string.substr(colon_pos + 1);
  } else {
    m_path = id_string;
  }
}

resource_id_t::resource_id_t(std::string_view namespace_name,
                             std::string_view path)
    : m_namespace(namespace_name), m_path(path) {}

auto resource_id_t::get_namespace() const -> const std::string & {
  return m_namespace;
}

auto resource_id_t::get_path() const -> const std::string & { return m_path; }

auto resource_id_t::to_string() const -> std::string {
  return m_namespace + ":" + m_path;
}

auto resource_id_t::operator==(const resource_id_t &other) const -> bool {
  return m_namespace == other.m_namespace && m_path == other.m_path;
}

auto resource_id_t::operator!=(const resource_id_t &other) const -> bool {
  return !(*this == other);
}

auto resource_id_t::operator<(const resource_id_t &other) const -> bool {
  if (m_namespace != other.m_namespace) {
    return m_namespace < other.m_namespace;
  }
  return m_path < other.m_path;
}

auto operator<<(std::ostream &os, const resource_id_t &id) -> std::ostream & {
  os << id.to_string();
  return os;
}

} // namespace deepbound
