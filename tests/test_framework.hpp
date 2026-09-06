#pragma once

#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace testfw {

using TestFunction = void (*)();

struct TestCase {
  std::string name;
  TestFunction function;
};

inline std::vector<TestCase>& registry() {
  static std::vector<TestCase> tests;
  return tests;
}

struct Registrar {
  Registrar(std::string name, TestFunction function) {
    registry().push_back(TestCase{std::move(name), function});
  }
};

inline void fail(const char* expression, const char* file, int line,
                 const std::string& detail = {}) {
  std::ostringstream out;
  out << file << ':' << line << ": assertion failed: " << expression;
  if (!detail.empty()) {
    out << " (" << detail << ')';
  }
  throw std::runtime_error(out.str());
}

template <typename A, typename B>
void require_equal(const A& actual, const B& expected, const char* actual_text,
                   const char* expected_text, const char* file, int line) {
  if (!(actual == expected)) {
    std::ostringstream detail;
    detail << actual_text << " != " << expected_text;
    fail("equality", file, line, detail.str());
  }
}

}  // namespace testfw

#define TEST_CASE(name)                                                       \
  static void test_##name();                                                  \
  static testfw::Registrar registrar_##name(#name, &test_##name);             \
  static void test_##name()

#define REQUIRE(expression)                                                   \
  do {                                                                        \
    if (!(expression)) {                                                      \
      testfw::fail(#expression, __FILE__, __LINE__);                          \
    }                                                                         \
  } while (false)

#define REQUIRE_EQ(actual, expected)                                          \
  testfw::require_equal((actual), (expected), #actual, #expected, __FILE__,   \
                        __LINE__)

#define REQUIRE_THROWS_AS(expression, exception_type)                         \
  do {                                                                        \
    bool caught_expected = false;                                             \
    try {                                                                     \
      static_cast<void>(expression);                                          \
    } catch (const exception_type&) {                                         \
      caught_expected = true;                                                 \
    } catch (...) {                                                           \
    }                                                                         \
    if (!caught_expected) {                                                   \
      testfw::fail("expected exception: " #exception_type, __FILE__,          \
                   __LINE__);                                                 \
    }                                                                         \
  } while (false)
