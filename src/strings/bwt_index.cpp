#include "algorithms/strings/bwt_index.hpp"

#include "algorithms/strings/suffix_array.hpp"

#include <algorithm>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>

namespace algorithms::strings {

BwtByteIndex::BwtByteIndex(std::string_view text) : BwtByteIndex(build(text)) {}

BwtByteIndex::BwtByteIndex(BuildState state)
    : text_size_(state.text_size),
      sentinel_row_(state.sentinel_row),
      cumulative_(state.cumulative),
      row_positions_(std::move(state.row_positions)),
      bwt_(std::span<const std::uint8_t>{state.bwt_bytes}) {}

BwtByteIndex::BuildState BwtByteIndex::build(std::string_view text) {
  const std::size_t size = text.size();
  if (size == std::numeric_limits<std::size_t>::max()) {
    throw std::length_error("BWT index row count overflow");
  }

  const SuffixArrayResult suffixes = build_suffix_array(text);
  if (suffixes.order.size() != size) {
    throw std::logic_error("BWT index suffix-array size invariant violated");
  }

  BuildState state;
  state.text_size = size;
  state.row_positions.reserve(size + 1U);
  state.row_positions.push_back(size);
  state.row_positions.insert(state.row_positions.end(), suffixes.order.begin(),
                             suffixes.order.end());

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
  for (std::size_t row = 0U; row < state.row_positions.size(); ++row) {
    const std::size_t start = state.row_positions[row];
    if (start > size) {
      throw std::logic_error("BWT index suffix-row position invariant violated");
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
  return state;
}

std::size_t BwtByteIndex::text_size() const noexcept { return text_size_; }

std::size_t BwtByteIndex::row_count() const noexcept {
  return row_positions_.size();
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

BwtByteIndex::SearchRange BwtByteIndex::backward_search(
    std::string_view pattern) const {
  SearchRange range{0U, row_count()};
  for (std::size_t remaining = pattern.size(); remaining > 0U; --remaining) {
    const std::uint8_t value = static_cast<std::uint8_t>(
        static_cast<unsigned char>(pattern[remaining - 1U]));
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
    range = SearchRange{next_begin, next_end};
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
    positions.push_back(row_positions_[row]);
  }
  std::sort(positions.begin(), positions.end());
  return positions;
}

}  // namespace algorithms::strings
