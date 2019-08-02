#ifndef AUTOCHECK_OSTREAM_HPP
#define AUTOCHECK_OSTREAM_HPP

#include <ostream>
#include <tuple>
#include <vector>

namespace autocheck {

  template <typename... Ts>
  std::ostream& operator<< (std::ostream& out, const std::tuple<Ts...>& tup);

  template <typename T>
  std::ostream& operator<< (std::ostream& out, const std::vector<T>& seq);

}

#endif

