#pragma once

#include "algorithms/automata/regex_derivative.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using algorithms::automata::ByteRegex;

namespace regex_derivative_test_detail {

enum class SpecKind : unsigned char {
  empty_set,
  epsilon,
  literal,
  alternate,
  concatenate,
  star,
};

struct Spec;
using SpecPtr = std::shared_ptr<const Spec>;

struct Spec {
  SpecKind kind;
  std::uint8_t byte{};
  SpecPtr left;
  SpecPtr right;
};

inline SpecPtr spec_leaf(const SpecKind kind) {
  return std::make_shared<const Spec>(Spec{kind, 0U, nullptr, nullptr});
}

inline SpecPtr spec_literal(const std::uint8_t byte) {
  return std::make_shared<const Spec>(
      Spec{SpecKind::literal, byte, nullptr, nullptr});
}

inline SpecPtr spec_binary(
    const SpecKind kind, SpecPtr left, SpecPtr right) {
  return std::make_shared<const Spec>(
      Spec{kind, 0U, std::move(left), std::move(right)});
}

inline SpecPtr spec_star(SpecPtr child) {
  return std::make_shared<const Spec>(
      Spec{SpecKind::star, 0U, std::move(child), nullptr});
}

inline ByteRegex build_regex(const SpecPtr& spec) {
  switch (spec->kind) {
    case SpecKind::empty_set:
      return ByteRegex::empty();
    case SpecKind::epsilon:
      return ByteRegex::epsilon();
    case SpecKind::literal:
      return ByteRegex::literal(spec->byte);
    case SpecKind::alternate:
      return ByteRegex::alternate(
          build_regex(spec->left), build_regex(spec->right));
    case SpecKind::concatenate:
      return ByteRegex::concatenate(
          build_regex(spec->left), build_regex(spec->right));
    case SpecKind::star:
      return ByteRegex::star(build_regex(spec->left));
  }
  return ByteRegex::empty();
}

inline std::set<std::size_t> oracle_ends(
    const SpecPtr& spec,
    const std::string_view input,
    const std::size_t begin) {
  switch (spec->kind) {
    case SpecKind::empty_set:
      return {};

    case SpecKind::epsilon:
      return {begin};

    case SpecKind::literal:
      if (begin < input.size() &&
          static_cast<std::uint8_t>(
              static_cast<unsigned char>(input[begin])) == spec->byte) {
        return {begin + 1U};
      }
      return {};

    case SpecKind::alternate: {
      auto result = oracle_ends(spec->left, input, begin);
      const auto right = oracle_ends(spec->right, input, begin);
      result.insert(right.begin(), right.end());
      return result;
    }

    case SpecKind::concatenate: {
      std::set<std::size_t> result;
      const auto middles = oracle_ends(spec->left, input, begin);
      for (const std::size_t middle : middles) {
        const auto ends = oracle_ends(spec->right, input, middle);
        result.insert(ends.begin(), ends.end());
      }
      return result;
    }

    case SpecKind::star: {
      std::set<std::size_t> reached{begin};
      std::vector<std::size_t> queue{begin};
      for (std::size_t head = 0U; head < queue.size(); ++head) {
        const auto ends =
            oracle_ends(spec->left, input, queue[head]);
        for (const std::size_t end : ends) {
          if (reached.insert(end).second) {
            queue.push_back(end);
          }
        }
      }
      return reached;
    }
  }
  return {};
}

inline bool oracle_matches(
    const SpecPtr& spec, const std::string_view input) {
  return oracle_ends(spec, input, 0U).contains(input.size());
}

inline SpecPtr random_spec(
    std::mt19937_64& random, const std::size_t depth) {
  if (depth == 0U) {
    const std::uint64_t pick = random() % 5U;
    if (pick == 0U) {
      return spec_leaf(SpecKind::empty_set);
    }
    if (pick == 1U) {
      return spec_leaf(SpecKind::epsilon);
    }
    return spec_literal(
        static_cast<std::uint8_t>(random() % 3U));
  }

  switch (random() % 7U) {
    case 0U:
      return spec_leaf(SpecKind::empty_set);
    case 1U:
      return spec_leaf(SpecKind::epsilon);
    case 2U:
      return spec_literal(
          static_cast<std::uint8_t>(random() % 3U));
    case 3U:
    case 4U:
      return spec_binary(
          SpecKind::alternate,
          random_spec(random, depth - 1U),
          random_spec(random, depth - 1U));
    case 5U:
      return spec_binary(
          SpecKind::concatenate,
          random_spec(random, depth - 1U),
          random_spec(random, depth - 1U));
    default:
      return spec_star(random_spec(random, depth - 1U));
  }
}

inline void enumerate_words(
    const std::size_t remaining,
    std::string& current,
    std::vector<std::string>& output) {
  output.push_back(current);
  if (remaining == 0U) {
    return;
  }
  for (unsigned char byte = 0U; byte < 3U; ++byte) {
    current.push_back(static_cast<char>(byte));
    enumerate_words(remaining - 1U, current, output);
    current.pop_back();
  }
}

}  // namespace regex_derivative_test_detail

TEST_CASE(regex_derivative_basic_language_contract) {
  const ByteRegex empty = ByteRegex::empty();
  const ByteRegex epsilon = ByteRegex::epsilon();
  const ByteRegex a = ByteRegex::literal(
      static_cast<std::uint8_t>('a'));
  const ByteRegex b = ByteRegex::literal(
      static_cast<std::uint8_t>('b'));

  REQUIRE(!empty.nullable());
  REQUIRE(epsilon.nullable());
  REQUIRE(!a.nullable());

  REQUIRE(!empty.matches(""));
  REQUIRE(epsilon.matches(""));
  REQUIRE(!epsilon.matches("a"));
  REQUIRE(a.matches("a"));
  REQUIRE(!a.matches(""));
  REQUIRE(!a.matches("aa"));

  const ByteRegex ab = ByteRegex::concatenate(a, b);
  REQUIRE(ab.matches("ab"));
  REQUIRE(!ab.matches("a"));
  REQUIRE(!ab.matches("ba"));

  const ByteRegex either = ByteRegex::alternate(a, b);
  REQUIRE(either.matches("a"));
  REQUIRE(either.matches("b"));
  REQUIRE(!either.matches("ab"));

  const ByteRegex repeated = ByteRegex::star(either);
  REQUIRE(repeated.matches(""));
  REQUIRE(repeated.matches("abbaab"));
  REQUIRE(!repeated.matches("abc"));

  const ByteRegex optional_a =
      ByteRegex::alternate(epsilon, a);
  const ByteRegex optional_then_b =
      ByteRegex::concatenate(optional_a, b);
  REQUIRE(optional_then_b.matches("b"));
  REQUIRE(optional_then_b.matches("ab"));
  REQUIRE(!optional_then_b.matches("a"));
  REQUIRE(!optional_then_b.matches("abb"));
}

TEST_CASE(regex_derivative_smart_constructor_identities) {
  const ByteRegex empty = ByteRegex::empty();
  const ByteRegex epsilon = ByteRegex::epsilon();
  const ByteRegex x = ByteRegex::literal(7U);

  REQUIRE_EQ(ByteRegex::alternate(empty, x).node_count(), 1U);
  REQUIRE_EQ(ByteRegex::alternate(x, x).node_count(), 1U);
  REQUIRE_EQ(ByteRegex::concatenate(epsilon, x).node_count(), 1U);
  REQUIRE_EQ(ByteRegex::concatenate(x, epsilon).node_count(), 1U);
  REQUIRE_EQ(ByteRegex::concatenate(empty, x).node_count(), 1U);
  REQUIRE_EQ(ByteRegex::star(empty).node_count(), 1U);
  REQUIRE_EQ(ByteRegex::star(epsilon).node_count(), 1U);
  REQUIRE_EQ(ByteRegex::star(ByteRegex::star(x)).node_count(), 2U);
}

TEST_CASE(regex_derivative_known_suffix_language) {
  const ByteRegex a = ByteRegex::literal(
      static_cast<std::uint8_t>('a'));
  const ByteRegex b = ByteRegex::literal(
      static_cast<std::uint8_t>('b'));
  const ByteRegex alphabet = ByteRegex::alternate(a, b);
  const ByteRegex expression = ByteRegex::concatenate(
      ByteRegex::star(alphabet),
      ByteRegex::concatenate(
          a, ByteRegex::concatenate(b, b)));

  REQUIRE(expression.matches("abb"));
  REQUIRE(expression.matches("aabb"));
  REQUIRE(expression.matches("babababb"));
  REQUIRE(!expression.matches("ab"));
  REQUIRE(!expression.matches("abba"));
  REQUIRE(!expression.matches("abcabb"));
}

TEST_CASE(regex_derivative_preserves_arbitrary_byte_values) {
  std::string input;
  input.push_back(static_cast<char>(0x00));
  input.push_back(static_cast<char>(0xFF));

  const ByteRegex expression = ByteRegex::concatenate(
      ByteRegex::literal(0U),
      ByteRegex::literal(255U));

  REQUIRE(expression.matches(input));
  REQUIRE(!expression.matches(std::string_view(input).substr(0U, 1U)));

  const ByteRegex after_zero = expression.derivative(0U);
  std::string tail;
  tail.push_back(static_cast<char>(0xFF));
  REQUIRE(after_zero.matches(tail));
}

TEST_CASE(regex_derivative_left_quotient_identity) {
  const ByteRegex a = ByteRegex::literal(
      static_cast<std::uint8_t>('a'));
  const ByteRegex b = ByteRegex::literal(
      static_cast<std::uint8_t>('b'));
  const ByteRegex expression = ByteRegex::concatenate(
      ByteRegex::star(ByteRegex::alternate(a, b)),
      ByteRegex::concatenate(a, b));

  const ByteRegex after_a =
      expression.derivative(static_cast<std::uint8_t>('a'));

  const std::vector<std::string> suffixes{
      "", "b", "ab", "aab", "bbb", "babab"};
  for (const std::string& suffix : suffixes) {
    std::string prefixed = "a";
    prefixed += suffix;
    REQUIRE(
        expression.matches(prefixed) ==
        after_a.matches(suffix));
  }
}

TEST_CASE(regex_derivative_randomized_matches_position_oracle) {
  using namespace regex_derivative_test_detail;

  std::mt19937_64 random(0xB22020551ULL);
  std::vector<std::string> words;
  std::string current;
  enumerate_words(4U, current, words);

  for (std::size_t trial = 0U; trial < 320U; ++trial) {
    const SpecPtr spec = random_spec(random, 3U);
    const ByteRegex expression = build_regex(spec);

    for (const std::string& word : words) {
      REQUIRE(
          expression.matches(word) ==
          oracle_matches(spec, word));

      if (!word.empty()) {
        const auto first = static_cast<std::uint8_t>(
            static_cast<unsigned char>(word.front()));
        const std::string_view suffix(word.data() + 1U, word.size() - 1U);
        REQUIRE(
            expression.matches(word) ==
            expression.derivative(first).matches(suffix));
      }
    }
  }
}
