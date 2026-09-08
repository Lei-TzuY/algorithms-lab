#pragma once

#include "algorithms/data_structures/packed_rank_select.hpp"
#include "algorithms/data_structures/run_length_byte_rank.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace algorithms::strings {

class BidirectionalBwtByteIndex;

struct HammingBwtSearchResult {
  std::vector<std::size_t> positions;
  std::size_t expanded_states = 0U;
  std::size_t transitions_considered = 0U;
  std::size_t pruned_empty_transitions = 0U;
  std::size_t terminal_states = 0U;
  std::size_t peak_frontier_size = 0U;
};

struct BwtTextReconstructionResult {
  std::string text;
  std::size_t lf_steps = 0U;
};

struct BwtTextExtractionResult {
  std::string bytes;
  std::size_t lf_steps = 0U;
  std::size_t reconstructed_bytes = 0U;
};

struct MemoizedRunSampledLocateResult {
  std::vector<std::size_t> positions;
  std::size_t seeded_row_count = 0U;
  std::size_t lf_steps = 0U;
  std::size_t memoized_row_count = 0U;
  std::size_t matched_row_cache_hits = 0U;
};

class BwtByteIndex {
 public:
  static constexpr std::size_t kDefaultLocateSampleRate = 32U;

  explicit BwtByteIndex(
      std::string_view text,
      std::size_t locate_sample_rate = kDefaultLocateSampleRate);

  [[nodiscard]] std::size_t text_size() const noexcept;
  [[nodiscard]] std::size_t row_count() const noexcept;

  // Reconstruct the exact constructor text from resident BWT/LF state.
  // No retained source-text copy is consulted. The baseline performs exactly
  // text_size() LF steps and allocates O(text_size()) result storage.
  [[nodiscard]] BwtTextReconstructionResult reconstruct_text() const;

  // Return constructor-text bytes in the validated half-open range [begin,end).
  // This first baseline reconstructs the full text before slicing, so the
  // diagnostics intentionally expose O(text_size()) query work even for a
  // short range.
  [[nodiscard]] BwtTextExtractionResult extract_text(std::size_t begin,
                                                      std::size_t end) const;

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

  // Enumerate the complete exact match set without consulting Phase-20
  // periodic locate samples. Every matched BWT row is LF-walked until it
  // reaches either the conceptual sentinel or a Phase-22 run-boundary sample.
  // The result is sorted by text position. This intentionally trades slower
  // worst-case query time for O(R) resident run-boundary sampling state.
  [[nodiscard]] std::vector<std::size_t> locate_run_sampled(
      std::string_view pattern) const;

  // Enumerate the same complete exact match set while memoizing conceptual-row
  // suffix positions only for this query. The cache is seeded exclusively by
  // the conceptual sentinel plus Phase-22 run-boundary samples; Phase-20
  // periodic locate samples are not consulted. Each newly traversed LF source
  // row is filled into the query cache when its path reaches a known row.
  [[nodiscard]] MemoizedRunSampledLocateResult locate_run_sampled_memoized(
      std::string_view pattern) const;

 private:
  friend class BidirectionalBwtByteIndex;

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

class BidirectionalBwtState {
 public:
  [[nodiscard]] std::size_t forward_begin() const noexcept;
  [[nodiscard]] std::size_t forward_end() const noexcept;
  [[nodiscard]] std::size_t reverse_begin() const noexcept;
  [[nodiscard]] std::size_t reverse_end() const noexcept;
  [[nodiscard]] std::size_t match_count() const noexcept;

 private:
  friend class BidirectionalBwtByteIndex;

  BidirectionalBwtState(std::size_t forward_begin, std::size_t forward_end,
                        std::size_t reverse_begin,
                        std::size_t reverse_end) noexcept;

  std::size_t forward_begin_ = 0U;
  std::size_t forward_end_ = 0U;
  std::size_t reverse_begin_ = 0U;
  std::size_t reverse_end_ = 0U;
};

// Exact bidirectional substring-search state over arbitrary bytes. A state is
// tied to the index that produced it; passing a state from another index is
// outside the API contract. The empty state represents the empty pattern and
// therefore has n+1 conceptual suffix rows in both directions.
class BidirectionalBwtByteIndex {
 public:
  explicit BidirectionalBwtByteIndex(std::string_view text);

  [[nodiscard]] std::size_t text_size() const noexcept;
  [[nodiscard]] std::size_t row_count() const noexcept;
  [[nodiscard]] BidirectionalBwtState empty_state() const noexcept;

  [[nodiscard]] BidirectionalBwtState extend_left(
      const BidirectionalBwtState& state, std::uint8_t value) const;
  [[nodiscard]] BidirectionalBwtState extend_right(
      const BidirectionalBwtState& state, std::uint8_t value) const;

  // Exact fixed-length Hamming-distance search. Only substitutions are
  // permitted; insertion/deletion edit distance is outside this API. The
  // deterministic center-out search reuses both bidirectional extension
  // directions and exposes search/pruning diagnostics rather than hiding the
  // potentially exponential branch frontier.
  [[nodiscard]] HammingBwtSearchResult locate_hamming(
      std::string_view pattern, std::size_t max_substitutions) const;

 private:
  void validate_state(const BidirectionalBwtState& state) const;
  [[nodiscard]] std::size_t extension_offset(
      const BwtByteIndex& index, BwtByteIndex::SearchRange range,
      std::uint8_t value) const;
  [[nodiscard]] static BwtByteIndex::SearchRange project_peer_interval(
      BwtByteIndex::SearchRange peer, std::size_t offset,
      std::size_t match_count);

  std::size_t text_size_ = 0U;
  BwtByteIndex forward_;
  BwtByteIndex reverse_;
};

}  // namespace algorithms::strings
