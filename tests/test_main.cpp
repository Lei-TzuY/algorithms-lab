#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct RunnerOptions {
  bool list_only = false;
  std::string filter;
};

void print_usage(const char* program) {
  std::cerr << "usage: " << program << " [--list] [--filter <substring>]\n";
}

bool parse_options(int argc, char* argv[], RunnerOptions& options) {
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (argument == "--list") {
      options.list_only = true;
      continue;
    }
    if (argument == "--filter") {
      if (index + 1 >= argc || !options.filter.empty()) {
        return false;
      }
      ++index;
      options.filter = argv[index];
      continue;
    }
    constexpr std::string_view prefix = "--filter=";
    if (argument.starts_with(prefix) && options.filter.empty()) {
      options.filter = std::string(argument.substr(prefix.size()));
      continue;
    }
    return false;
  }
  return true;
}

bool selected(const testfw::TestCase& test, const std::string& filter) {
  return filter.empty() || test.name.find(filter) != std::string::npos;
}

}  // namespace

int main(int argc, char* argv[]) {
  RunnerOptions options;
  if (!parse_options(argc, argv, options)) {
    print_usage(argv[0]);
    return 2;
  }

  std::vector<testfw::TestCase> tests = testfw::registry();
  std::sort(tests.begin(), tests.end(), [](const auto& left, const auto& right) {
    return left.name < right.name;
  });

  for (std::size_t index = 1; index < tests.size(); ++index) {
    if (tests[index - 1].name == tests[index].name) {
      std::cerr << "duplicate test name: " << tests[index].name << '\n';
      return 2;
    }
  }

  std::size_t selected_count = 0;
  for (const auto& test : tests) {
    if (!selected(test, options.filter)) {
      continue;
    }
    ++selected_count;
    if (options.list_only) {
      std::cout << test.name << '\n';
    }
  }

  if (selected_count == 0) {
    std::cerr << "no tests matched filter: " << options.filter << '\n';
    return 2;
  }
  if (options.list_only) {
    return 0;
  }

  std::size_t passed = 0;
  for (const auto& test : tests) {
    if (!selected(test, options.filter)) {
      continue;
    }
    try {
      test.function();
      ++passed;
      std::cout << "[PASS] " << test.name << '\n';
    } catch (const std::exception& error) {
      std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
      return 1;
    } catch (...) {
      std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
      return 1;
    }
  }
  std::cout << passed << " tests passed\n";
  return 0;
}
