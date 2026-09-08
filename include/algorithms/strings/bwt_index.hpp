#pragma once

#include "algorithms/data_structures/packed_rank_select.hpp"
#include "algorithms/data_structures/run_length_byte_rank.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace algorithms::strings {

class BwtByteIndex {
 public:
  static constexpr std::size_t kDefaultLocateSampleRate = 32U;

  explicit BwtByteIndex(
      std::string_view text,
      std::size_t locate_sample_rate = kDefaultLocateSampleRate);

  [[nodiscard]] std::size_t text_size() const noexcept;
  [[nodiscard]] std::size_t row_count() const noexcept;

  // Locate sampling diagnostics. The row-membership payload is the logical
  // packed-bit/rank payload only; sample-position bytes are the stored
  // size_t values only. Allocator/vector object overhead is excluded.
  [[nodiscard]] std::size_t locate_sample_rate() const noexcept;
  [[nodiscard]] std::size_t sampled_row_count() const noexcept;
  [[nodiscard]] std::size_t sampled_membership_payload_bytes() const noexcept;
  [[nodiscard]] std::size_t sampled_position_payload_bytes() const noexcept;
  [[nodiscard]] std::size_t sampled_locate_payload_bytes() const noexcept;
  [[nodiscard]] std::size_t max_lf_steps_per_locate() const noexcept;

  // BWT occurrence diagnostics. Payload is the logical bytes occupied by the
  // run arrays and symbol-run index entries only; vector/allocator overhead
  // and spare capacity are intentionally excluded.
  [[nodiscard]] std::size_t bwt_run_count() const noexcept;
  [[nodiscard]] std::size_t bwt_occurrence_payload_bytes() const noexcept;

  // Run-aware toehold diagnostics. Samples are suffix positions stored at
  // full conceptual-BWT byte-run boundaries; the sentinel is an explicit run
  // break. Payload counts the two size_t vectors only.
  [[nodiscard]] std::size_t run_toehold_sample_count() const noexcept;
  [[nodiscard]] std::size_t run_toehold_sample_payload_bytes() const noexcept;

  // Exact substring search over arbitrary bytes. The empty pattern matches
  // every boundary position 0..text_size(), matching the repository's KMP
  // convention.
  [[nodiscard]] std::size_t count(std::string_view pattern) const;
  [[nodiscard]] std::vector<std::size_t> locate(std::string_view pattern) const;

  // Return one exact match through a BWT-run toehold rather than through the
  // Phase-20 periodic locate samples. Empty pattern deterministically returns
  // boundary zero; a non-empty absent pattern returns nullopt.
  [[nodiscard]] std::optional<std::size_t> locate_one_toehold(
      std::string_view pattern) const;

 private:
  struct SearchRange {
    std::size_t begin;
    std::size_t end;
  };

  struct BuildState {
    std::size_t text_size = 0U;
    std::size_t locate_sample_rate = 0U;
    std::size_t sentinel_row = 0U;
    std::size_t sampled_position_payload_bytes = 0U;
    std::size_t sampled_locate_payload_bytes = 0U;
    std::size_t run_toehold_sample_payload_bytes = 0U;
    std::array<std::size_t, 256U> cumulative{};
    std::vector<std::uint8_t> sampled_rows;
    std::vector<std::size_t> sampled_positions;
    std::vector<std::size_t> run_toehold_sample_rows;
    std::vector<std::size_t> run_toehold_sample_positions;
    std::vector<std::uint8_t> bwt_bytes;
  };

  explicit BwtByteIndex(BuildState state);
  [[nodiscard]] static BuildState build(std::string_view text,
                                        std::size_t locate_sample_rate);
  [[nodiscard]] SearchRange backward_search(std::string_view pattern) const;
  [[nodiscard]] SearchRange extend(SearchRange range,
                                   std::uint8_t value) const;
  [[nodiscard]] std::size_t occurrence(std::uint8_t value,
                                       std::size_t row_end) const;
  [[nodiscard]] std::uint8_t bwt_byte_at_row(std::size_t row) const;
  [[nodiscard]] std::size_t full_row_for_byte_occurrence(
      std::uint8_t value, std::size_t ordinal) const;
  [[nodiscard]] std::size_t run_toehold_sample_position(
      std::size_t row) const;
  [[nodiscard]] std::size_t lf(std::size_t row) const;
  [[nodiscard]] std::size_t resolve_row_position(std::size_t row) const;

  std::size_t text_size_ = 0U;
  std::size_t locate_sample_rate_ = 0U;
  std::size_t sentinel_row_ = 0U;
  std::size_t sampled_position_payload_bytes_ = 0U;
  std::size_t sampled_locate_payload_bytes_ = 0U;
  std::size_t run_toehold_sample_payload_bytes_ = 0U;
  std::array<std::size_t, 256U> cumulative_{};
  algorithms::data_structures::PackedRankSelectBitVector sampled_rows_;
  std::vector<std::size_t> sampled_positions_;
  std::vector<std::size_t> run_toehold_sample_rows_;
  std::vector<std::size_t> run_toehold_sample_positions_;
  algorithms::data_structures::RunLengthByteRankIndex bwt_;
};

}  // namespace algorithms::strings
