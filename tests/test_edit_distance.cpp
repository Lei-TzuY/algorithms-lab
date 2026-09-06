#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "algorithms/dynamic_programming/edit_distance.hpp"

namespace {

using algorithms::dynamic_programming::EditDistanceResult;
using algorithms::dynamic_programming::EditKind;

std::string apply_script(std::string_view source,
                         const EditDistanceResult& result) {
  std::string output;
  std::size_t source_index = 0;
  std::size_t edit_count = 0;
  for (const auto& operation : result.operations) {
    switch (operation.kind) {
      case EditKind::match:
        REQUIRE(source_index < source.size());
        REQUIRE_EQ(operation.source_character, source[source_index]);
        REQUIRE_EQ(operation.target_character, source[source_index]);
        output.push_back(source[source_index]);
        ++source_index;
        break;
      case EditKind::substitute:
        REQUIRE(source_index < source.size());
        REQUIRE_EQ(operation.source_character, source[source_index]);
        output.push_back(operation.target_character);
        ++source_index;
        ++edit_count;
        break;
      case EditKind::erase:
        REQUIRE(source_index < source.size());
        REQUIRE_EQ(operation.source_character, source[source_index]);
        ++source_index;
        ++edit_count;
        break;
      case EditKind::insert:
        output.push_back(operation.target_character);
        ++edit_count;
        break;
    }
  }
  REQUIRE_EQ(source_index, source.size());
  REQUIRE_EQ(edit_count, result.distance);
  return output;
}

std::size_t two_row_oracle(std::string_view source, std::string_view target) {
  std::vector<std::size_t> previous(target.size() + 1);
  std::vector<std::size_t> current(target.size() + 1);
  for (std::size_t j = 0; j <= target.size(); ++j) {
    previous[j] = j;
  }
  for (std::size_t i = 1; i <= source.size(); ++i) {
    current[0] = i;
    for (std::size_t j = 1; j <= target.size(); ++j) {
      if (source[i - 1] == target[j - 1]) {
        current[j] = previous[j - 1];
      } else {
        current[j] =
            1 + std::min(previous[j - 1], std::min(previous[j], current[j - 1]));
      }
    }
    previous.swap(current);
  }
  return previous[target.size()];
}

std::string random_string(std::mt19937_64& rng, std::size_t length) {
  std::uniform_int_distribution<int> character_distribution(0, 3);
  std::string result;
  result.reserve(length);
  for (std::size_t index = 0; index < length; ++index) {
    result.push_back(static_cast<char>('a' + character_distribution(rng)));
  }
  return result;
}

}  // namespace

TEST_CASE(edit_distance_handles_empty_identical_and_classic_examples) {
  const auto empty =
      algorithms::dynamic_programming::levenshtein_edit_distance("", "");
  REQUIRE_EQ(empty.distance, std::size_t{0});
  REQUIRE(empty.operations.empty());

  const auto insert_only =
      algorithms::dynamic_programming::levenshtein_edit_distance("", "abc");
  REQUIRE_EQ(insert_only.distance, std::size_t{3});
  REQUIRE_EQ(apply_script("", insert_only), std::string{"abc"});

  const auto erase_only =
      algorithms::dynamic_programming::levenshtein_edit_distance("abc", "");
  REQUIRE_EQ(erase_only.distance, std::size_t{3});
  REQUIRE_EQ(apply_script("abc", erase_only), std::string{});

  const auto identical = algorithms::dynamic_programming::levenshtein_edit_distance(
      "algorithm", "algorithm");
  REQUIRE_EQ(identical.distance, std::size_t{0});
  REQUIRE_EQ(apply_script("algorithm", identical), std::string{"algorithm"});

  const auto kitten = algorithms::dynamic_programming::levenshtein_edit_distance(
      "kitten", "sitting");
  REQUIRE_EQ(kitten.distance, std::size_t{3});
  REQUIRE_EQ(apply_script("kitten", kitten), std::string{"sitting"});

  const auto flaw =
      algorithms::dynamic_programming::levenshtein_edit_distance("flaw", "lawn");
  REQUIRE_EQ(flaw.distance, std::size_t{2});
  REQUIRE_EQ(apply_script("flaw", flaw), std::string{"lawn"});
}

TEST_CASE(edit_distance_uses_deterministic_substitution_tie_breaking) {
  const auto result =
      algorithms::dynamic_programming::levenshtein_edit_distance("ab", "ba");
  REQUIRE_EQ(result.distance, std::size_t{2});
  REQUIRE_EQ(result.operations.size(), std::size_t{2});
  REQUIRE(result.operations[0].kind == EditKind::substitute);
  REQUIRE(result.operations[1].kind == EditKind::substitute);
  REQUIRE_EQ(apply_script("ab", result), std::string{"ba"});
}

TEST_CASE(edit_distance_handles_embedded_null_bytes) {
  const std::string source{"a\0b", 3};
  const std::string target{"a\0c", 3};
  const auto result = algorithms::dynamic_programming::levenshtein_edit_distance(
      std::string_view{source.data(), source.size()},
      std::string_view{target.data(), target.size()});
  REQUIRE_EQ(result.distance, std::size_t{1});
  REQUIRE_EQ(apply_script(source, result), target);
}

TEST_CASE(edit_distance_matches_two_row_oracle_and_symmetry_randomized) {
  std::mt19937_64 rng(0xED17D15AULL);
  std::uniform_int_distribution<int> length_distribution(0, 10);

  for (std::size_t trial = 0; trial < 520; ++trial) {
    const std::string source = random_string(
        rng, static_cast<std::size_t>(length_distribution(rng)));
    const std::string target = random_string(
        rng, static_cast<std::size_t>(length_distribution(rng)));

    const auto forward =
        algorithms::dynamic_programming::levenshtein_edit_distance(source, target);
    const auto reverse =
        algorithms::dynamic_programming::levenshtein_edit_distance(target, source);
    REQUIRE_EQ(forward.distance, two_row_oracle(source, target));
    REQUIRE_EQ(forward.distance, reverse.distance);
    REQUIRE_EQ(apply_script(source, forward), target);
    REQUIRE_EQ(apply_script(target, reverse), source);

    const std::size_t length_gap = source.size() > target.size()
                                       ? source.size() - target.size()
                                       : target.size() - source.size();
    REQUIRE(forward.distance >= length_gap);
    REQUIRE(forward.distance <= std::max(source.size(), target.size()));
  }
}

TEST_CASE(edit_distance_satisfies_triangle_inequality_randomized) {
  std::mt19937_64 rng(0x7A1A6EULL);
  std::uniform_int_distribution<int> length_distribution(0, 8);

  for (std::size_t trial = 0; trial < 260; ++trial) {
    const std::string a =
        random_string(rng, static_cast<std::size_t>(length_distribution(rng)));
    const std::string b =
        random_string(rng, static_cast<std::size_t>(length_distribution(rng)));
    const std::string c =
        random_string(rng, static_cast<std::size_t>(length_distribution(rng)));
    const std::size_t ab =
        algorithms::dynamic_programming::levenshtein_edit_distance(a, b).distance;
    const std::size_t bc =
        algorithms::dynamic_programming::levenshtein_edit_distance(b, c).distance;
    const std::size_t ac =
        algorithms::dynamic_programming::levenshtein_edit_distance(a, c).distance;
    REQUIRE(ac <= ab + bc);
  }
}
