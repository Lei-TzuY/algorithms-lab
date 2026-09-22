#pragma once

#include "algorithms/combinatorics/de_bruijn.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

using algorithms::combinatorics::de_bruijn_sequence;

namespace de_bruijn_test_detail {

inline std::size_t checked_power(
    const std::size_t base,
    const std::size_t exponent) {
  std::size_t value = 1U;
  for (std::size_t i = 0U; i < exponent; ++i) {
    REQUIRE(base == 0U ||
            value <= std::numeric_limits<std::size_t>::max() / base);
    value *= base;
  }
  return value;
}

// Definition-level oracle: every cyclic length-order window is encoded as a
// base-k integer. Exact de Bruijn validity means all k^n codes appear once.
inline void require_exact_de_bruijn(
    const std::vector<std::size_t>& sequence,
    const std::size_t alphabet_size,
    const std::size_t order) {
  REQUIRE(alphabet_size >= 1U);
  REQUIRE(order >= 1U);

  const std::size_t expected =
      checked_power(alphabet_size, order);
  REQUIRE_EQ(sequence.size(), expected);

  std::vector<bool> seen(expected, false);
  for (std::size_t start = 0U; start < sequence.size(); ++start) {
    std::size_t code = 0U;
    for (std::size_t offset = 0U; offset < order; ++offset) {
      const std::size_t symbol =
          sequence[(start + offset) % sequence.size()];
      REQUIRE(symbol < alphabet_size);
      code = code * alphabet_size + symbol;
    }
    REQUIRE(code < seen.size());
    REQUIRE(!seen[code]);
    seen[code] = true;
  }

  REQUIRE(std::all_of(
      seen.begin(), seen.end(),
      [](const bool value) { return value; }));
}

}  // namespace de_bruijn_test_detail

TEST_CASE(de_bruijn_rejects_invalid_parameters_and_overflow) {
  REQUIRE_THROWS_AS(de_bruijn_sequence(0U, 1U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(de_bruijn_sequence(2U, 0U),
                    std::invalid_argument);

  const std::size_t digits =
      std::numeric_limits<std::size_t>::digits;
  REQUIRE_THROWS_AS(de_bruijn_sequence(2U, digits),
                    std::length_error);
  REQUIRE_THROWS_AS(
      de_bruijn_sequence(
          std::numeric_limits<std::size_t>::max(), 2U),
      std::length_error);
}

TEST_CASE(de_bruijn_single_symbol_all_orders) {
  using namespace de_bruijn_test_detail;

  for (const std::size_t order : {1U, 2U, 3U, 17U, 1024U}) {
    const auto sequence = de_bruijn_sequence(1U, order);
    REQUIRE(sequence == std::vector<std::size_t>{0U});
    require_exact_de_bruijn(sequence, 1U, order);
  }
}

TEST_CASE(de_bruijn_known_binary_order_three_fkm_output) {
  using namespace de_bruijn_test_detail;

  const auto sequence = de_bruijn_sequence(2U, 3U);
  const std::vector<std::size_t> expected{
      0U, 0U, 0U, 1U, 0U, 1U, 1U, 1U};

  REQUIRE(sequence == expected);
  require_exact_de_bruijn(sequence, 2U, 3U);
}

TEST_CASE(de_bruijn_small_parameter_grid_matches_definition) {
  using namespace de_bruijn_test_detail;

  for (std::size_t alphabet_size = 1U;
       alphabet_size <= 5U; ++alphabet_size) {
    for (std::size_t order = 1U; order <= 5U; ++order) {
      const auto first =
          de_bruijn_sequence(alphabet_size, order);
      const auto second =
          de_bruijn_sequence(alphabet_size, order);

      REQUIRE(first == second);
      require_exact_de_bruijn(
          first, alphabet_size, order);
    }
  }
}

TEST_CASE(de_bruijn_nonbinary_medium_cases_match_definition) {
  using namespace de_bruijn_test_detail;

  for (const auto& [alphabet_size, order] :
       std::vector<std::pair<std::size_t, std::size_t>>{
           {3U, 6U}, {4U, 5U}, {7U, 4U}}) {
    const auto sequence =
        de_bruijn_sequence(alphabet_size, order);
    require_exact_de_bruijn(
        sequence, alphabet_size, order);
  }
}
