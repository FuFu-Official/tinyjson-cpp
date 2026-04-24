#pragma once

#include <cstddef>
#include <iostream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace tinyjson {
namespace print_detail {

template <class T> struct always_false : std::false_type {};

inline void write_indent(std::size_t indent) {
  for (std::size_t i = 0; i < indent; ++i) {
    std::cout << ' ';
  }
}

template <class T> void print_impl(T const &value, std::size_t indent);

inline void print_value(std::nullptr_t, std::size_t) { std::cout << "null"; }

inline void print_value(bool value, std::size_t) {
  std::cout << (value ? "true" : "false");
}

inline void print_value(int value, std::size_t) { std::cout << value; }

inline void print_value(double value, std::size_t) { std::cout << value; }

inline void print_value(std::string const &value, std::size_t) {
  std::cout << '"' << value << '"';
}

template <class T>
void print_value(std::vector<T> const &list, std::size_t indent) {
  std::cout << '[';
  if (!list.empty()) {
    for (std::size_t i = 0; i < list.size(); ++i) {
      print_impl(list[i], indent);
      if (i + 1 != list.size()) {
        std::cout << ", ";
      }
    }
  }
  std::cout << ']';
}

template <class V>
void print_value(std::unordered_map<std::string, V> const &dict,
                 std::size_t indent) {
  std::cout << '{';
  if (!dict.empty()) {
    std::cout << '\n';
    std::size_t i = 0;
    for (auto const &[key, value] : dict) {
      write_indent(indent + 2);
      std::cout << '"' << key << "\": ";
      print_impl(value, indent + 2);
      if (i + 1 != dict.size()) {
        std::cout << ',';
      }
      std::cout << '\n';
      ++i;
    }
    write_indent(indent);
  }
  std::cout << '}';
}

template <class... Ts>
void print_value(std::variant<Ts...> const &value, std::size_t indent) {
  std::visit([indent](auto const &inner) { print_impl(inner, indent); }, value);
}

template <class T>
concept has_inner_member = requires(T const &value) { value.inner; };

template <has_inner_member T>
void print_value(T const &value, std::size_t indent) {
  print_impl(value.inner, indent);
}

template <class T> void print_impl(T const &value, std::size_t indent) {
  print_value(value, indent);
}

template <class T> void print_value(T const &, std::size_t) {
  static_assert(always_false<T>::value, "print(): unsupported type");
}
} // namespace print_detail

template <typename T> void print(T const &value) {
  print_detail::print_impl(value, 0);
}

template <typename T> void printnl(T const &value) {
  print(value);
  std::cout << '\n';
}

} // namespace tinyjson
