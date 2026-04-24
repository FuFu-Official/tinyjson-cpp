#include "CLI11.hpp"
#include "tinyjson.hpp"
#include <filesystem>
#include <iostream>
#include <string>

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

  tinyjson::JSONObject obj = tinyjson::parse(raw_json).first;
  print(obj);

  return 0;
}
