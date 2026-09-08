#include "algorithms/strings/bwt_index.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string make_memoized_locate_bytes(
    std::initializer_list<unsigned int> values) {
  std::string result;
  result.reserve(values.size());
  for (const unsigned int value : values) {
    REQUIRE(value <= 255U);
    result.push_back(
        static_cast<char>(static_cast<unsigned char>(value)));
  }
  return result;
}

std::string random_memoized_locate_bytes(std::mt19937_64& rng,
                                         std::size_t length,
                                         unsigned int alphabet) {
  std::uniform_int_distribution<unsigned int> byte_dist(0U, alphabet - 1U);
  std::string result(length, '\0');
  for (std::size_t index = 0U; index < length; ++index) {
    result[index] =
        static_cast<char>(static_cast<unsigned char>(byte_dist(rng)));
  }
  return result;
}

std::vector<std::size_t> direct_memoized_locations(std::string_view text,
                                                   std::string_view pattern) {
  if (pattern.empty()) {
    std::vector<std::size_t> positions(text.size() + 1U);
    for (std::size_t position = 0U; position <= text.size(); ++position) {
      positions[position] = position;
    }
    return positions;
  }
  if (pattern.size() > text.size()) {
    return {};
  }

  std::vector<std::size_t> positions;
  const std::size_t last = text.size() - pattern.size();
  for (std::size_t start = 0U; start <= last; ++start) {
    if (text.substr(start, pattern.size()) == pattern) {
      positions.push_back(start);
    }
  }
  return positions;
}

void require_memoized_diagnostics(
    const algorithms::strings::BwtByteIndex& index,
    const algorithms::strings::MemoizedRunSampledLocateResult& result,
    std::size_t expected_match_count) {
  REQUIRE_EQ(result.seeded_row_count,
             index.run_toehold_sample_count() + 1U);
  REQUIRE(result.seeded_row_count <= index.row_count());
  REQUIRE(result.memoized_row_count >= result.seeded_row_count);
  REQUIRE(result.memoized_row_count <= index.row_count());
  REQUIRE_EQ(result.lf_steps,
             result.memoized_row_count - result.seeded_row_count);
  REQUIRE(result.lf_steps <= index.row_count() - result.seeded_row_count);
  REQUIRE(result.matched_row_cache_hits <= expected_match_count);
}

void require_memoized_locations(
    const algorithms::strings::BwtByteIndex& index, std::string_view text,
    std::string_view pattern) {
  const std::vector<std::size_t> expected =
      direct_memoized_locations(text, pattern);
  const auto result = index.locate_run_sampled_memoized(pattern);
  REQUIRE_EQ(result.positions, expected);
  require_memoized_diagnostics(index, result, expected.size());
}

void require_same_memoized_diagnostics(
    const algorithms::strings::MemoizedRunSampledLocateResult& lhs,
    const algorithms::strings::MemoizedRunSampledLocateResult& rhs) {
  REQUIRE_EQ(lhs.positions, rhs.positions);
  REQUIRE_EQ(lhs.seeded_row_count, rhs.seeded_row_count);
  REQUIRE_EQ(lhs.lf_steps, rhs.lf_steps);
  REQUIRE_EQ(lhs.memoized_row_count, rhs.memoized_row_count);
  REQUIRE_EQ(lhs.matched_row_cache_hits, rhs.matched_row_cache_hits);
}

}  // namespace

TEST_CASE(bwt_memoized_run_sampled_locate_deterministic_and_diagnostic) {
  using algorithms::strings::BwtByteIndex;

  const BwtByteIndex empty{std::string_view{}, 17U};
  const auto empty_all =
      empty.locate_run_sampled_memoized(std::string_view{});
  REQUIRE_EQ(empty_all.positions, (std::vector<std::size_t>{0U}));
  REQUIRE_EQ(empty_all.seeded_row_count, 1U);
  REQUIRE_EQ(empty_all.lf_steps, 0U);
  REQUIRE_EQ(empty_all.memoized_row_count, 1U);
  REQUIRE_EQ(empty_all.matched_row_cache_hits, 1U);

  const auto empty_absent = empty.locate_run_sampled_memoized("x");
  REQUIRE(empty_absent.positions.empty());
  REQUIRE_EQ(empty_absent.seeded_row_count, 1U);
  REQUIRE_EQ(empty_absent.lf_steps, 0U);
  REQUIRE_EQ(empty_absent.memoized_row_count, 1U);
  REQUIRE_EQ(empty_absent.matched_row_cache_hits, 0U);

  const std::string banana = "banana";
  const BwtByteIndex dense{banana, 1U};
  const BwtByteIndex sparse{banana, 1024U};
  REQUIRE(dense.sampled_row_count() > sparse.sampled_row_count());
  REQUIRE_EQ(dense.run_toehold_sample_count(),
             sparse.run_toehold_sample_count());

  for (const std::string_view pattern :
       {std::string_view{}, std::string_view{"a"}, std::string_view{"ana"},
        std::string_view{"banana"}, std::string_view{"nana"},
        std::string_view{"x"}}) {
    const std::vector<std::size_t> expected =
        direct_memoized_locations(banana, pattern);
    const auto dense_result = dense.locate_run_sampled_memoized(pattern);
    const auto sparse_result = sparse.locate_run_sampled_memoized(pattern);
    REQUIRE_EQ(dense_result.positions, expected);
    REQUIRE_EQ(sparse_result.positions, expected);
    require_memoized_diagnostics(dense, dense_result, expected.size());
    require_memoized_diagnostics(sparse, sparse_result, expected.size());
    require_same_memoized_diagnostics(dense_result, sparse_result);
    REQUIRE_EQ(sparse_result.positions, sparse.locate_run_sampled(pattern));
  }

  const auto full = sparse.locate_run_sampled_memoized(std::string_view{});
  REQUIRE_EQ(full.memoized_row_count, sparse.row_count());
  REQUIRE_EQ(full.lf_steps, sparse.row_count() - full.seeded_row_count);
  REQUIRE(full.matched_row_cache_hits >= full.seeded_row_count);

  const std::string repetitive(128U, 'a');
  const BwtByteIndex repetitive_index{repetitive, 4096U};
  for (const std::string_view pattern :
       {std::string_view{}, std::string_view{"a"}, std::string_view{"aaaa"},
        std::string_view{"aaaaaaaaaaaaaaaa"}, std::string_view{"b"}}) {
    require_memoized_locations(repetitive_index, repetitive, pattern);
  }

  const std::string sentinel_split =
      make_memoized_locate_bytes({0U, 1U, 1U});
  const BwtByteIndex split_index{sentinel_split, 128U};
  for (const std::string& pattern :
       {make_memoized_locate_bytes({0U}), make_memoized_locate_bytes({1U}),
        make_memoized_locate_bytes({1U, 1U}),
        make_memoized_locate_bytes({0U, 1U}),
        make_memoized_locate_bytes({1U, 0U})}) {
    require_memoized_locations(split_index, sentinel_split, pattern);
  }

  const std::string arbitrary = make_memoized_locate_bytes(
      {255U, 0U, 128U, 255U, 0U, 128U, 0U});
  const BwtByteIndex arbitrary_index{arbitrary, 257U};
  for (const std::string& pattern :
       {make_memoized_locate_bytes({255U}), make_memoized_locate_bytes({0U}),
        make_memoized_locate_bytes({128U}),
        make_memoized_locate_bytes({255U, 0U}),
        make_memoized_locate_bytes({0U, 128U}),
        make_memoized_locate_bytes({128U, 255U}),
        make_memoized_locate_bytes({255U, 255U})}) {
    require_memoized_locations(arbitrary_index, arbitrary, pattern);
  }
}

TEST_CASE(bwt_memoized_run_sampled_locate_randomized_differential) {
  using algorithms::strings::BwtByteIndex;

  std::mt19937_64 rng(0x24B0175AULL);
  std::uniform_int_distribution<std::size_t> text_length_dist(0U, 80U);
  std::uniform_int_distribution<std::size_t> pattern_length_dist(0U, 16U);
  std::uniform_int_distribution<unsigned int> alphabet_choice(0U, 3U);

  for (std::size_t trial = 0U; trial < 350U; ++trial) {
    const unsigned int choice = alphabet_choice(rng);
    const unsigned int alphabet = choice == 0U ? 256U : 1U << choice;
    const std::string text = random_memoized_locate_bytes(
        rng, text_length_dist(rng), alphabet);

    const BwtByteIndex dense{text, 1U};
    const BwtByteIndex sparse{text, text.size() + 257U};
    REQUIRE_EQ(dense.run_toehold_sample_count(),
               sparse.run_toehold_sample_count());

    const auto dense_full =
        dense.locate_run_sampled_memoized(std::string_view{});
    const auto sparse_full =
        sparse.locate_run_sampled_memoized(std::string_view{});
    require_same_memoized_diagnostics(dense_full, sparse_full);
    REQUIRE_EQ(sparse_full.memoized_row_count, sparse.row_count());
    REQUIRE_EQ(sparse_full.lf_steps,
               sparse.row_count() - sparse_full.seeded_row_count);

    for (std::size_t query = 0U; query < 60U; ++query) {
      std::string pattern;
      if (query % 3U == 0U) {
        std::uniform_int_distribution<std::size_t> start_dist(0U,
                                                               text.size());
        const std::size_t start = start_dist(rng);
        const std::size_t maximum =
            std::min<std::size_t>(16U, text.size() - start);
        std::uniform_int_distribution<std::size_t> length_dist(0U, maximum);
        pattern = text.substr(start, length_dist(rng));
      } else {
        pattern = random_memoized_locate_bytes(
            rng, pattern_length_dist(rng), alphabet);
      }

      const std::vector<std::size_t> expected =
          direct_memoized_locations(text, pattern);
      const auto dense_result = dense.locate_run_sampled_memoized(pattern);
      const auto sparse_result = sparse.locate_run_sampled_memoized(pattern);
      REQUIRE_EQ(dense_result.positions, expected);
      REQUIRE_EQ(sparse_result.positions, expected);
      require_memoized_diagnostics(dense, dense_result, expected.size());
      require_memoized_diagnostics(sparse, sparse_result, expected.size());
      require_same_memoized_diagnostics(dense_result, sparse_result);

      if (query % 17U == 0U) {
        REQUIRE_EQ(sparse_result.positions,
                   sparse.locate_run_sampled(pattern));
      }
    }
  }
}
