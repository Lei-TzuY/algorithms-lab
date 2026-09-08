#include "algorithms/data_structures/byte_wavelet_matrix.hpp"
#include "algorithms/strings/bwt_index.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <string>
#include <vector>

TEST_CASE(bwt_index_run_length_occurrence_diagnostics_on_repetitive_text) {
  const std::string text(2048U, 'a');
  const algorithms::strings::BwtByteIndex index{text, 32U};

  REQUIRE_EQ(index.bwt_run_count(), std::size_t{1});
  REQUIRE_EQ(index.bwt_occurrence_payload_bytes(),
             std::size_t{3} * sizeof(std::size_t) + sizeof(std::uint8_t));

  std::vector<std::uint8_t> repeated_bwt(text.size(),
                                         static_cast<std::uint8_t>('a'));
  const algorithms::data_structures::ByteWaveletMatrix wavelet_baseline{
      std::span<const std::uint8_t>{repeated_bwt}};
  REQUIRE(index.bwt_occurrence_payload_bytes() <
          wavelet_baseline.logical_payload_bytes());

  std::vector<std::size_t> expected(text.size() - 3U + 1U);
  std::iota(expected.begin(), expected.end(), std::size_t{0});
  REQUIRE_EQ(index.count("aaa"), expected.size());
  REQUIRE_EQ(index.locate("aaa"), expected);
}

TEST_CASE(bwt_index_run_length_occurrence_preserves_sampled_locate_contract) {
  const std::string text = "mississippi_mississippi";
  for (const std::size_t rate : {1U, 2U, 5U, 16U, 64U}) {
    const algorithms::strings::BwtByteIndex index{text, rate};
    REQUIRE(index.bwt_run_count() > 0U);
    REQUIRE(index.bwt_run_count() <= text.size());
    REQUIRE_EQ(index.locate_sample_rate(), rate);
    REQUIRE_EQ(index.max_lf_steps_per_locate(), rate - 1U);
    REQUIRE_EQ(index.count("issi"), std::size_t{4});
    REQUIRE_EQ(index.locate("issi"),
               (std::vector<std::size_t>{1U, 4U, 13U, 16U}));
  }
}
