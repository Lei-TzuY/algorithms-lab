#include "algorithms/strings/bwt_index.hpp"

#include "algorithms/strings/suffix_array.hpp"

#include <algorithm>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>

namespace algorithms::strings {
namespace {

std::size_t checked_add(std::size_t lhs, std::size_t rhs) {
  if (rhs > std::numeric_limits<std::size_t>::max() - lhs) {
    throw std::length_error("BWT payload size overflow");
  }
  return lhs + rhs;
}

std::size_t checked_mul(std::size_t lhs, std::size_t rhs) {
  if (lhs != 0U && rhs > std::numeric_limits<std::size_t>::max() / lhs) {
    throw std::length_error("BWT payload size overflow");
  }
  return lhs * rhs;
}

}  // namespace

BwtByteIndex::BwtByteIndex(std::string_view text,
                           std::size_t locate_sample_rate)
    : BwtByteIndex(build(text, locate_sample_rate)) {}

BwtByteIndex::BwtByteIndex(BuildState state)
    : text_size_(state.text_size),
      locate_sample_rate_(state.locate_sample_rate),
      sentinel_row_(state.sentinel_row),
      sampled_position_payload_bytes_(state.sampled_position_payload_bytes),
      sampled_locate_payload_bytes_(state.sampled_locate_payload_bytes),
      run_toehold_sample_payload_bytes_(
          state.run_toehold_sample_payload_bytes),
      cumulative_(state.cumulative),
      sampled_rows_(std::span<const std::uint8_t>{state.sampled_rows}),
      sampled_positions_(std::move(state.sampled_positions)),
      run_toehold_sample_rows_(std::move(state.run_toehold_sample_rows)),
      run_toehold_sample_positions_(
          std::move(state.run_toehold_sample_positions)),
      bwt_(std::span<const std::uint8_t>{state.bwt_bytes}) {
  if (sampled_rows_.size() != row_count() ||
      sampled_rows_.one_count() != sampled_positions_.size()) {
    throw std::logic_error("BWT sampled-row cardinality invariant violated");
  }
  if (run_toehold_sample_rows_.size() !=
      run_toehold_sample_positions_.size()) {
    throw std::logic_error("BWT run-toehold sample cardinality invariant violated");
  }
  for (std::size_t index = 0U; index < run_toehold_sample_rows_.size();
       ++index) {
    const std::size_t row = run_toehold_sample_rows_[index];
    const std::size_t position = run_toehold_sample_positions_[index];
    if (row >= row_count() || row == sentinel_row_ || position == 0U ||
        position > text_size_) {
      throw std::logic_error("BWT run-toehold sample invariant violated");
    }
    if (index > 0U && run_toehold_sample_rows_[index - 1U] >= row) {
      throw std::logic_error("BWT run-toehold sample rows not increasing");
    }
  }
}

BwtByteIndex::BuildState BwtByteIndex::build(
    std::string_view text, std::size_t locate_sample_rate) {
  if (locate_sample_rate == 0U) {
    throw std::invalid_argument("BWT locate sample rate must be positive");
  }

  const std::size_t size = text.size();
  if (size == std::numeric_limits<std::size_t>::max()) {
    throw std::length_error("BWT index row count overflow");
  }

  const SuffixArrayResult suffixes = build_suffix_array(text);
  if (suffixes.order.size() != size) {
    throw std::logic_error("BWT index suffix-array size invariant violated");
  }

  std::vector<std::size_t> row_positions;
  row_positions.reserve(size + 1U);
  row_positions.push_back(size);
  row_positions.insert(row_positions.end(), suffixes.order.begin(),
                       suffixes.order.end());

  BuildState state;
  state.text_size = size;
  state.locate_sample_rate = locate_sample_rate;
  state.sampled_rows.assign(size + 1U, 0U);

  std::array<std::size_t, 256U> frequencies{};
  for (const char ch : text) {
    const std::uint8_t value =
        static_cast<std::uint8_t>(static_cast<unsigned char>(ch));
    ++frequencies[static_cast<std::size_t>(value)];
  }

  std::size_t prefix = 1U;  // The conceptual sentinel is smaller than all bytes.
  for (std::size_t symbol = 0U; symbol < frequencies.size(); ++symbol) {
    state.cumulative[symbol] = prefix;
    prefix += frequencies[symbol];
  }
  if (prefix != size + 1U) {
    throw std::logic_error("BWT index cumulative-count invariant violated");
  }

  state.bwt_bytes.reserve(size);
  bool found_sentinel = false;
  for (std::size_t row = 0U; row < row_positions.size(); ++row) {
    const std::size_t start = row_positions[row];
    if (start > size) {
      throw std::logic_error("BWT index suffix-row position invariant violated");
    }

    const bool sampled =
        start == size || start % locate_sample_rate == 0U;
    if (sampled) {
      state.sampled_rows[row] = 1U;
      state.sampled_positions.push_back(start);
    }

    if (start == 0U) {
      state.sentinel_row = row;
      found_sentinel = true;
      continue;
    }

    const char previous = text[start - 1U];
    state.bwt_bytes.push_back(
        static_cast<std::uint8_t>(static_cast<unsigned char>(previous)));
  }

  if (!found_sentinel || state.bwt_bytes.size() != size) {
    throw std::logic_error("BWT index sentinel-row invariant violated");
  }
  if (state.sampled_positions.empty()) {
    throw std::logic_error("BWT sampled-locate requires at least one sample");
  }

  const auto full_bwt_byte = [&](std::size_t row) -> std::uint8_t {
    if (row >= row_positions.size() || row == state.sentinel_row) {
      throw std::logic_error("BWT run-toehold byte-row invariant violated");
    }
    const std::size_t suffix_position = row_positions[row];
    if (suffix_position == 0U || suffix_position > size) {
      throw std::logic_error("BWT run-toehold suffix-position invariant violated");
    }
    return static_cast<std::uint8_t>(
        static_cast<unsigned char>(text[suffix_position - 1U]));
  };

  for (std::size_t row = 0U; row < row_positions.size(); ++row) {
    if (row == state.sentinel_row) {
      continue;
    }
    const std::uint8_t value = full_bwt_byte(row);
    const bool starts_run =
        row == 0U || row - 1U == state.sentinel_row ||
        full_bwt_byte(row - 1U) != value;
    const bool ends_run =
        row + 1U == row_positions.size() ||
        row + 1U == state.sentinel_row || full_bwt_byte(row + 1U) != value;
    if (starts_run || ends_run) {
      state.run_toehold_sample_rows.push_back(row);
      state.run_toehold_sample_positions.push_back(row_positions[row]);
    }
  }

  if (state.run_toehold_sample_rows.size() !=
      state.run_toehold_sample_positions.size()) {
    throw std::logic_error("BWT run-toehold build cardinality invariant violated");
  }

  const algorithms::data_structures::PackedRankSelectBitVector sampled_rows(
      std::span<const std::uint8_t>{state.sampled_rows});
  if (sampled_rows.one_count() != state.sampled_positions.size()) {
    throw std::logic_error("BWT sampled-row build invariant violated");
  }

  state.sampled_position_payload_bytes =
      checked_mul(state.sampled_positions.size(), sizeof(std::size_t));
  state.sampled_locate_payload_bytes = checked_add(
      sampled_rows.logical_payload_bytes(), state.sampled_position_payload_bytes);
  const std::size_t run_sample_rows_bytes = checked_mul(
      state.run_toehold_sample_rows.size(), sizeof(std::size_t));
  const std::size_t run_sample_positions_bytes = checked_mul(
      state.run_toehold_sample_positions.size(), sizeof(std::size_t));
  state.run_toehold_sample_payload_bytes =
      checked_add(run_sample_rows_bytes, run_sample_positions_bytes);
  return state;
}

std::size_t BwtByteIndex::text_size() const noexcept { return text_size_; }

std::size_t BwtByteIndex::row_count() const noexcept { return text_size_ + 1U; }

std::size_t BwtByteIndex::locate_sample_rate() const noexcept {
  return locate_sample_rate_;
}

std::size_t BwtByteIndex::sampled_row_count() const noexcept {
  return sampled_positions_.size();
}

std::size_t BwtByteIndex::sampled_membership_payload_bytes() const noexcept {
  return sampled_rows_.logical_payload_bytes();
}

std::size_t BwtByteIndex::sampled_position_payload_bytes() const noexcept {
  return sampled_position_payload_bytes_;
}

std::size_t BwtByteIndex::sampled_locate_payload_bytes() const noexcept {
  return sampled_locate_payload_bytes_;
}

std::size_t BwtByteIndex::max_lf_steps_per_locate() const noexcept {
  return locate_sample_rate_ - 1U;
}

std::size_t BwtByteIndex::bwt_run_count() const noexcept {
  return bwt_.run_count();
}

std::size_t BwtByteIndex::bwt_occurrence_payload_bytes() const noexcept {
  return bwt_.logical_payload_bytes();
}

std::size_t BwtByteIndex::run_toehold_sample_count() const noexcept {
  return run_toehold_sample_rows_.size();
}

std::size_t BwtByteIndex::run_toehold_sample_payload_bytes() const noexcept {
  return run_toehold_sample_payload_bytes_;
}

std::size_t BwtByteIndex::occurrence(std::uint8_t value,
                                     std::size_t row_end) const {
  if (row_end > row_count()) {
    throw std::logic_error("BWT index occurrence endpoint invariant violated");
  }

  std::size_t compressed_end = row_end;
  if (sentinel_row_ < row_end) {
    --compressed_end;
  }
  return bwt_.rank(value, compressed_end);
}

std::uint8_t BwtByteIndex::bwt_byte_at_row(std::size_t row) const {
  if (row >= row_count() || row == sentinel_row_) {
    throw std::logic_error("BWT byte-row invariant violated");
  }
  std::size_t compressed_row = row;
  if (sentinel_row_ < row) {
    --compressed_row;
  }
  return bwt_.access(compressed_row);
}

std::size_t BwtByteIndex::full_row_for_byte_occurrence(
    std::uint8_t value, std::size_t ordinal) const {
  const std::size_t compressed_row = bwt_.select(value, ordinal);
  const std::size_t full_row =
      compressed_row + (compressed_row >= sentinel_row_ ? 1U : 0U);
  if (full_row >= row_count() || full_row == sentinel_row_) {
    throw std::logic_error("BWT selected byte-row invariant violated");
  }
  return full_row;
}

std::size_t BwtByteIndex::run_toehold_sample_position(
    std::size_t row) const {
  const auto sample = std::lower_bound(run_toehold_sample_rows_.begin(),
                                       run_toehold_sample_rows_.end(), row);
  if (sample == run_toehold_sample_rows_.end() || *sample != row) {
    throw std::logic_error("BWT run-toehold boundary sample missing");
  }
  const std::size_t ordinal = static_cast<std::size_t>(
      std::distance(run_toehold_sample_rows_.begin(), sample));
  if (ordinal >= run_toehold_sample_positions_.size()) {
    throw std::logic_error("BWT run-toehold sample ordinal invariant violated");
  }
  return run_toehold_sample_positions_[ordinal];
}

std::size_t BwtByteIndex::lf(std::size_t row) const {
  if (row >= row_count()) {
    throw std::logic_error("BWT LF source row invariant violated");
  }
  if (row == sentinel_row_) {
    return 0U;
  }

  const std::uint8_t value = bwt_byte_at_row(row);
  const std::size_t base = cumulative_[static_cast<std::size_t>(value)];
  const std::size_t before = occurrence(value, row);
  if (base > row_count() - before) {
    throw std::logic_error("BWT LF-mapping invariant violated");
  }
  const std::size_t next = base + before;
  if (next >= row_count()) {
    throw std::logic_error("BWT LF target row invariant violated");
  }
  return next;
}

std::size_t BwtByteIndex::resolve_row_position(std::size_t row) const {
  if (row >= row_count()) {
    throw std::logic_error("BWT locate row invariant violated");
  }

  std::size_t current = row;
  std::size_t steps = 0U;
  while (!sampled_rows_.bit(current)) {
    current = lf(current);
    ++steps;
    if (steps >= locate_sample_rate_) {
      throw std::logic_error("BWT locate LF sample bound violated");
    }
  }

  const std::size_t sample_ordinal = sampled_rows_.rank1(current);
  if (sample_ordinal >= sampled_positions_.size()) {
    throw std::logic_error("BWT sampled position ordinal invariant violated");
  }
  const std::size_t sample_position = sampled_positions_[sample_ordinal];
  if (sample_position > text_size_ || steps > text_size_ - sample_position) {
    throw std::logic_error("BWT sampled position reconstruction invariant violated");
  }
  return sample_position + steps;
}

BwtByteIndex::SearchRange BwtByteIndex::extend(SearchRange range,
                                               std::uint8_t value) const {
  if (range.begin > range.end || range.end > row_count()) {
    throw std::logic_error("BWT index search range invariant violated");
  }
  const std::size_t base = cumulative_[static_cast<std::size_t>(value)];
  const std::size_t begin_occurrence = occurrence(value, range.begin);
  const std::size_t end_occurrence = occurrence(value, range.end);
  if (base > row_count() - begin_occurrence ||
      base > row_count() - end_occurrence) {
    throw std::logic_error("BWT index LF-mapping invariant violated");
  }

  const std::size_t next_begin = base + begin_occurrence;
  const std::size_t next_end = base + end_occurrence;
  if (next_begin > next_end || next_end > row_count()) {
    throw std::logic_error("BWT index backward-search interval invalid");
  }
  return SearchRange{next_begin, next_end};
}

BwtByteIndex::SearchRange BwtByteIndex::backward_search(
    std::string_view pattern) const {
  SearchRange range{0U, row_count()};
  for (std::size_t remaining = pattern.size(); remaining > 0U; --remaining) {
    const std::uint8_t value = static_cast<std::uint8_t>(
        static_cast<unsigned char>(pattern[remaining - 1U]));
    range = extend(range, value);
    if (range.begin == range.end) {
      break;
    }
  }
  return range;
}

std::size_t BwtByteIndex::count(std::string_view pattern) const {
  const SearchRange range = backward_search(pattern);
  return range.end - range.begin;
}

std::vector<std::size_t> BwtByteIndex::locate(std::string_view pattern) const {
  const SearchRange range = backward_search(pattern);
  std::vector<std::size_t> positions;
  positions.reserve(range.end - range.begin);
  for (std::size_t row = range.begin; row < range.end; ++row) {
    positions.push_back(resolve_row_position(row));
  }
  std::sort(positions.begin(), positions.end());
  return positions;
}

std::optional<std::size_t> BwtByteIndex::locate_one_toehold(
    std::string_view pattern) const {
  if (pattern.empty()) {
    return std::size_t{0};
  }

  SearchRange range{0U, row_count()};
  std::size_t toehold_row = 0U;
  std::size_t toehold_position = text_size_;

  for (std::size_t remaining = pattern.size(); remaining > 0U; --remaining) {
    if (toehold_row < range.begin || toehold_row >= range.end) {
      throw std::logic_error("BWT run-toehold interval invariant violated");
    }

    const std::uint8_t value = static_cast<std::uint8_t>(
        static_cast<unsigned char>(pattern[remaining - 1U]));
    const SearchRange next = extend(range, value);
    if (next.begin == next.end) {
      return std::nullopt;
    }

    if (toehold_row != sentinel_row_ &&
        bwt_byte_at_row(toehold_row) == value) {
      if (toehold_position == 0U) {
        throw std::logic_error("BWT run-toehold zero-position byte row");
      }
      toehold_row = lf(toehold_row);
      --toehold_position;
    } else {
      const std::size_t through_toehold = occurrence(value, toehold_row + 1U);
      const std::size_t through_range = occurrence(value, range.end);
      std::size_t boundary_row = 0U;

      if (through_range > through_toehold) {
        boundary_row =
            full_row_for_byte_occurrence(value, through_toehold);
      } else {
        const std::size_t before_toehold = occurrence(value, toehold_row);
        const std::size_t before_range = occurrence(value, range.begin);
        if (before_toehold <= before_range) {
          throw std::logic_error("BWT run-toehold side invariant violated");
        }
        boundary_row =
            full_row_for_byte_occurrence(value, before_toehold - 1U);
      }

      if (boundary_row < range.begin || boundary_row >= range.end) {
        throw std::logic_error("BWT run-toehold boundary outside interval");
      }
      const std::size_t boundary_position =
          run_toehold_sample_position(boundary_row);
      if (boundary_position == 0U) {
        throw std::logic_error("BWT run-toehold sampled sentinel position");
      }
      toehold_row = lf(boundary_row);
      toehold_position = boundary_position - 1U;
    }

    if (toehold_row < next.begin || toehold_row >= next.end) {
      throw std::logic_error("BWT run-toehold extension invariant violated");
    }
    range = next;
  }

  return toehold_position;
}

}  // namespace algorithms::strings
