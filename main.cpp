#include "CLI11.hpp"
#include "print.hpp"
#include <charconv>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

struct JSONObject;

using JSONDICT = std::unordered_map<std::string, JSONObject>;
using JSONLIST = std::vector<JSONObject>;

struct JSONObject {
  std::variant<std::nullptr_t, // none
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

int main(int argc, char **argv) {
  CLI::App app{"a simple JSON parser"};

  std::string path;

  app.add_option("filepath", path, "JSON file to parse")->type_name("");

  CLI11_PARSE(app, argc, argv);

  std::filesystem::path fs(path);

  if (!std::filesystem::exists(fs)) {
    std::cerr << "Invalid path.";
    return -1;
  }

  std::ifstream infile(path);
  std::string raw_json;
  if (infile) {
    raw_json.assign((std::istreambuf_iterator<char>(infile)),
                    std::istreambuf_iterator<char>());
  }

  JSONObject obj = parse(raw_json).first;
  print(obj);

  return 0;
}

std::pair<JSONObject, size_t> parse(std::string_view json) {
  // Parse empty
  if (json.empty()) {
    return {JSONObject{std::nullptr_t{}}, 0};
  }

  // Exclude leading escape characters
  if (size_t off = json.find_first_not_of(" \n\r\t\v\f\0");
      off != 0 && off != json.npos) {
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

    size_t i = 1;
    for (; i < json.size(); ++i) {
      char ch = json[i];
      if (phase == Raw) {
        if (ch == '\\') {
          phase = Esc;
          continue;
        } else if (ch == '"') {
          i++;
          break;
        } else {
          str += ch;
        }
      }

      if (phase == Esc) {
        str += unescaped_char(ch);
        phase = Raw;
      }
    }

    return {JSONObject{std::move(str)}, i};
  }

  auto skip_whitespace = [&json](size_t &i) {
    while (i < json.size() &&
           std::isspace(static_cast<unsigned char>(json[i]))) {
      ++i;
    }
  };

  // Parse list
  if (json[0] == '[') {
    std::vector<JSONObject> res;
    size_t i;
    for (i = 1; i < json.size();) {
      if (json[i] == ']') {
        i += 1;
        break;
      }
      auto [obj, eaten] = parse(json.substr(i));
      if (eaten == 0) {
        i = 0;
        break;
      }
      res.push_back(std::move(obj));
      i += eaten;

      skip_whitespace(i);
      if (json[i] == ',') {
        i += 1;
      }
      skip_whitespace(i);
    }
    return {JSONObject{std::move(res)}, i};
  }

  // Parse dict
  if (json[0] == '{') {
    std::unordered_map<std::string, JSONObject> res;
    size_t i;
    for (i = 1; i < json.size();) {
      if (json[i] == '}') {
        i += 1;
        break;
      }
      auto [keyobj, keyeaten] = parse(json.substr(i));
      if (keyeaten == 0) {
        i = 0;
        break;
      }
      i += keyeaten;
      if (!std::holds_alternative<std::string>(keyobj.inner)) {
        i = 0;
        break;
      }

      skip_whitespace(i);
      if (json[i] == ':') {
        i += 1;
      }
      skip_whitespace(i);

      std::string key = std::move(std::get<std::string>(keyobj.inner));
      auto [valobj, valeaten] = parse(json.substr(i));
      if (valeaten == 0) {
        i = 0;
        break;
      }
      i += valeaten;
      res.try_emplace(std::move(key), std::move(valobj));

      skip_whitespace(i);
      if (json[i] == ',') {
        i += 1;
      }
      skip_whitespace(i);
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
