#include "tinyjson.hpp"
#include <charconv>
#include <cstddef>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace tinyjson {
namespace {

bool is_json_whitespace(char ch) {
  return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t';
}

bool is_hex_digit(char ch) {
  return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') ||
         (ch >= 'A' && ch <= 'F');
}

unsigned hex_value(char ch) {
  if (ch >= '0' && ch <= '9') {
    return static_cast<unsigned>(ch - '0');
  }
  if (ch >= 'a' && ch <= 'f') {
    return static_cast<unsigned>(10 + ch - 'a');
  }
  return static_cast<unsigned>(10 + ch - 'A');
}

bool parse_u4(std::string_view json, size_t start, unsigned &value) {
  if (start + 4 > json.size()) {
    return false;
  }

  value = 0;
  for (size_t j = 0; j < 4; ++j) {
    char ch = json[start + j];
    if (!is_hex_digit(ch)) {
      return false;
    }
    value = (value << 4U) | hex_value(ch);
  }

  return true;
}

void append_utf8(std::string &out, unsigned codepoint) {
  if (codepoint <= 0x7FU) {
    out.push_back(static_cast<char>(codepoint));
    return;
  }

  if (codepoint <= 0x7FFU) {
    out.push_back(static_cast<char>(0xC0U | (codepoint >> 6U)));
    out.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    return;
  }

  if (codepoint <= 0xFFFFU) {
    out.push_back(static_cast<char>(0xE0U | (codepoint >> 12U)));
    out.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
    out.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    return;
  }

  out.push_back(static_cast<char>(0xF0U | (codepoint >> 18U)));
  out.push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3FU)));
  out.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
  out.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
}

} // namespace

std::pair<JSONObject, size_t> parse(std::string_view json) {
  // Parse empty
  if (json.empty()) {
    return {JSONObject{std::nullptr_t{}}, 0};
  }

  // Exclude leading escape characters
  size_t off = 0;
  while (off < json.size() && is_json_whitespace(json[off])) {
    ++off;
  }
  if (off != 0) {
    auto [obj, eaten] = parse(json.substr(off));
    return {std::move(obj), eaten + off};
  }

  if (json.size() >= 4) {
    // Parse null
    if (json.substr(0, 4) == "null") {
      return {JSONObject{std::nullptr_t{}}, 4};
    }

    // Parse bool
    if (json.substr(0, 4) == "true") {
      return {JSONObject{true}, 4};
    }
  }

  if (json.size() >= 5) {
    if (json.substr(0, 5) == "false") {
      return {JSONObject{false}, 5};
    }
  }

  // Parse int & double
  if (char ch = json[0]; (ch >= '0' && ch <= '9') || ch == '+' || ch == '-') {
    std::regex num_regex{"-?(0|[1-9][0-9]*)(\\.[0-9]+)?([eE][+-]?[0-9]+)?"};
    std::cmatch match;
    if (std::regex_search(json.data(), json.data() + json.size(), match,
                          num_regex)) {
      std::string str = match.str();
      if (auto num = try_parse_num<int>(str); num.has_value()) {
        return {JSONObject{num.value()}, str.size()};
      }

      if (auto num = try_parse_num<double>(str); num.has_value()) {
        return {JSONObject{num.value()}, str.size()};
      }
    }
  }

  // Parse string
  if (json[0] == '"') {
    std::string str;
    enum { Raw, Esc } phase = Raw;
    bool closed = false;

    size_t i = 1;
    for (; i < json.size(); ++i) {
      char ch = json[i];
      if (phase == Raw) {
        if (ch == '\\') {
          phase = Esc;
          continue;
        } else if (ch == '"') {
          i++;
          closed = true;
          break;
        } else if (static_cast<unsigned char>(ch) < 0x20U) {
          return {JSONObject{std::nullptr_t{}}, 0};
        } else {
          str += ch;
        }
      }

      if (phase == Esc) {
        if (ch == '"' || ch == '\\' || ch == '/') {
          str += ch;
          phase = Raw;
          continue;
        }

        if (ch == 'b') {
          str += '\b';
          phase = Raw;
          continue;
        }

        if (ch == 'f') {
          str += '\f';
          phase = Raw;
          continue;
        }

        if (ch == 'n') {
          str += '\n';
          phase = Raw;
          continue;
        }

        if (ch == 'r') {
          str += '\r';
          phase = Raw;
          continue;
        }

        if (ch == 't') {
          str += '\t';
          phase = Raw;
          continue;
        }

        if (ch == 'u') {
          unsigned first = 0;
          if (!parse_u4(json, i + 1, first)) {
            return {JSONObject{std::nullptr_t{}}, 0};
          }

          if (first >= 0xD800U && first <= 0xDBFFU) {
            if (i + 10 >= json.size() || json[i + 5] != '\\' ||
                json[i + 6] != 'u') {
              return {JSONObject{std::nullptr_t{}}, 0};
            }

            unsigned second = 0;
            if (!parse_u4(json, i + 7, second) || second < 0xDC00U ||
                second > 0xDFFFU) {
              return {JSONObject{std::nullptr_t{}}, 0};
            }

            unsigned codepoint =
                0x10000U + (((first - 0xD800U) << 10U) | (second - 0xDC00U));
            append_utf8(str, codepoint);
            i += 10;
            phase = Raw;
            continue;
          }

          if (first >= 0xDC00U && first <= 0xDFFFU) {
            return {JSONObject{std::nullptr_t{}}, 0};
          }

          append_utf8(str, first);
          i += 4;
          phase = Raw;
          continue;
        }

        return {JSONObject{std::nullptr_t{}}, 0};
        phase = Raw;
      }
    }

    if (!closed) {
      return {JSONObject{std::nullptr_t{}}, 0};
    }

    return {JSONObject{std::move(str)}, i};
  }

  auto skip_whitespace = [&json](size_t &i) {
    while (i < json.size() && is_json_whitespace(json[i])) {
      ++i;
    }
  };

  // Parse list
  if (json[0] == '[') {
    std::vector<JSONObject> res;
    size_t i = 1;
    bool closed = false;

    skip_whitespace(i);
    if (i < json.size() && json[i] == ']') {
      return {JSONObject{std::move(res)}, i + 1};
    }

    for (; i < json.size();) {
      auto [obj, eaten] = parse(json.substr(i));
      if (eaten == 0) {
        return {JSONObject{std::nullptr_t{}}, 0};
      }
      res.push_back(std::move(obj));
      i += eaten;

      skip_whitespace(i);
      if (i >= json.size()) {
        return {JSONObject{std::nullptr_t{}}, 0};
      }

      if (json[i] == ',') {
        i += 1;
        skip_whitespace(i);
        if (i >= json.size() || json[i] == ']') {
          return {JSONObject{std::nullptr_t{}}, 0};
        }
        continue;
      }

      if (json[i] == ']') {
        i += 1;
        closed = true;
        break;
      }

      return {JSONObject{std::nullptr_t{}}, 0};
    }

    if (!closed) {
      return {JSONObject{std::nullptr_t{}}, 0};
    }

    return {JSONObject{std::move(res)}, i};
  }

  // Parse dict
  if (json[0] == '{') {
    std::unordered_map<std::string, JSONObject> res;
    size_t i = 1;
    bool closed = false;

    skip_whitespace(i);
    if (i < json.size() && json[i] == '}') {
      return {JSONObject{std::move(res)}, i + 1};
    }

    for (; i < json.size();) {
      auto [keyobj, keyeaten] = parse(json.substr(i));
      if (keyeaten == 0 || !std::holds_alternative<std::string>(keyobj.inner)) {
        return {JSONObject{std::nullptr_t{}}, 0};
      }
      i += keyeaten;

      skip_whitespace(i);
      if (i >= json.size() || json[i] != ':') {
        return {JSONObject{std::nullptr_t{}}, 0};
      }
      i += 1;
      skip_whitespace(i);

      std::string key = std::move(std::get<std::string>(keyobj.inner));
      auto [valobj, valeaten] = parse(json.substr(i));
      if (valeaten == 0) {
        return {JSONObject{std::nullptr_t{}}, 0};
      }
      i += valeaten;
      res.try_emplace(std::move(key), std::move(valobj));

      skip_whitespace(i);
      if (i >= json.size()) {
        return {JSONObject{std::nullptr_t{}}, 0};
      }

      if (json[i] == ',') {
        i += 1;
        skip_whitespace(i);
        if (i >= json.size() || json[i] == '}') {
          return {JSONObject{std::nullptr_t{}}, 0};
        }
        continue;
      }

      if (json[i] == '}') {
        i += 1;
        closed = true;
        break;
      }

      return {JSONObject{std::nullptr_t{}}, 0};
    }

    if (!closed) {
      return {JSONObject{std::nullptr_t{}}, 0};
    }

    return {JSONObject{std::move(res)}, i};
  }

  return {JSONObject{std::nullptr_t{}}, 0};
}

template <class T> std::optional<T> try_parse_num(std::string_view str) {
  T value;
  auto res = std::from_chars(str.data(), str.data() + str.size(), value);
  if (res.ec == std::errc() && res.ptr == str.data() + str.size()) {
    return value;
  }
  return std::nullopt;
}

char unescaped_char(char c) {
  switch (c) {
  case 'n':
    return '\n';
  case 'r':
    return '\r';
  case '0':
    return '\0';
  case 't':
    return '\t';
  case 'v':
    return '\v';
  case 'f':
    return '\f';
  case 'b':
    return '\b';
  case 'a':
    return '\a';
  default:
    return c;
  }
}
} // namespace tinyjson
