#pragma once

#include "print.hpp"
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace tinyjson {
struct JSONObject;

using JSONDICT = std::unordered_map<std::string, JSONObject>;
using JSONLIST = std::vector<JSONObject>;

struct JSONObject {
  std::variant<std::nullptr_t, // null
               bool,           // true & false
               int,            // 3
               double,         // 3,14
               std::string,    // "hello"
               JSONLIST,       // [true, 3]
               JSONDICT        // {"hello": 3}
               >
      inner;

  void do_print() const { printnl(inner); }

  template <class T> bool is() { return std::holds_alternative<T>(inner); }

  template <class T> T const &get() const { return std::get<T>(inner); }

  template <class T> T &get() { return std::get<T>(inner); }
};

char unescaped_char(char c);

template <class T> std::optional<T> try_parse_num(std::string_view str);

std::pair<JSONObject, size_t> parse(std::string_view json);
} // namespace tinyjson
