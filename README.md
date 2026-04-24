## Introduction

A simple JSON Parser.

## Supported JSON Types

- Number (`int` or `double`)
- Boolean
- Null
- String
- Array

## Error Handling

For invalid JSON input, the program exits with code `-1` after:

```cpp
std::cerr << "Invalid JSON format.";
```

## References

- [CLI11 - Command Line Parser Library](https://github.com/CLIUtils/CLI11)
