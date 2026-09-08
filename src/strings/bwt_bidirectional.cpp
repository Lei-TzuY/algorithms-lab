#include "algorithms/strings/bwt_index.hpp"

#include <stdexcept>
#include <string>

namespace algorithms::strings {
namespace {

std::string reversed_bytes(std::string_view text) {
  return std::string{text.rbegin(), text.rend()};
}

}  // namespace

BidirectionalBwtState::BidirectionalBwtState(
    std::size_t forward_begin, std::size_t forward_end,
    std::size_t reverse_begin, std::size_t reverse_end) noexcept
    : forward_begin_(forward_begin),
      forward_end_(forward_end),
      reverse_begin_(reverse_begin),
      reverse_end_(reverse_end) {}

std::size_t BidirectionalBwtState::forward_begin() const noexcept {
  return forward_begin_;
}

std::size_t BidirectionalBwtState::forward_end() const noexcept {
  return forward_end_;
}

std::size_t BidirectionalBwtState::reverse_begin() const noexcept {
  return reverse_begin_;
}

std::size_t BidirectionalBwtState::reverse_end() const noexcept {
  return reverse_end_;
}

std::size_t BidirectionalBwtState::match_count() const noexcept {
  return forward_end_ - forward_begin_;
}

BidirectionalBwtByteIndex::BidirectionalBwtByteIndex(std::string_view text)
    : text_size_(text.size()), forward_(text), reverse_(reversed_bytes(text)) {}

std::size_t BidirectionalBwtByteIndex::text_size() const noexcept {
  return text_size_;
}

std::size_t BidirectionalBwtByteIndex::row_count() const noexcept {
  return text_size_ + 1U;
}

BidirectionalBwtState BidirectionalBwtByteIndex::empty_state() const noexcept {
  return BidirectionalBwtState{0U, row_count(), 0U, row_count()};
}

void BidirectionalBwtByteIndex::validate_state(
    const BidirectionalBwtState& state) const {
  if (state.forward_begin_ > state.forward_end_ ||
      state.forward_end_ > forward_.row_count() ||
      state.reverse_begin_ > state.reverse_end_ ||
      state.reverse_end_ > reverse_.row_count()) {
    throw std::invalid_argument("bidirectional BWT state interval out of range");
  }
  const std::size_t forward_count =
      state.forward_end_ - state.forward_begin_;
  const std::size_t reverse_count =
      state.reverse_end_ - state.reverse_begin_;
  if (forward_count != reverse_count) {
    throw std::invalid_argument(
        "bidirectional BWT paired interval cardinality mismatch");
  }
}

std::size_t BidirectionalBwtByteIndex::extension_offset(
    const BwtByteIndex& index, BwtByteIndex::SearchRange range,
    std::uint8_t value) const {
  if (range.begin > range.end || range.end > index.row_count()) {
    throw std::logic_error("bidirectional BWT source interval invalid");
  }
  const std::size_t range_size = range.end - range.begin;
  std::size_t offset =
      range.begin <= index.sentinel_row_ && index.sentinel_row_ < range.end
          ? 1U
          : 0U;
  const std::size_t target = static_cast<std::size_t>(value);
  for (std::size_t symbol = 0U; symbol < target; ++symbol) {
    const auto byte = static_cast<std::uint8_t>(symbol);
    const std::size_t before = index.occurrence(byte, range.begin);
    const std::size_t after = index.occurrence(byte, range.end);
    if (after < before) {
      throw std::logic_error("bidirectional BWT occurrence order invalid");
    }
    const std::size_t delta = after - before;
    if (offset > range_size || delta > range_size - offset) {
      throw std::logic_error("bidirectional BWT extension partition overflow");
    }
    offset += delta;
  }
  if (offset > range_size) {
    throw std::logic_error("bidirectional BWT extension offset invalid");
  }
  return offset;
}

BwtByteIndex::SearchRange BidirectionalBwtByteIndex::project_peer_interval(
    BwtByteIndex::SearchRange peer, std::size_t offset,
    std::size_t match_count) {
  if (peer.begin > peer.end) {
    throw std::logic_error("bidirectional BWT peer interval invalid");
  }
  const std::size_t peer_count = peer.end - peer.begin;
  if (offset > peer_count || match_count > peer_count - offset) {
    throw std::logic_error("bidirectional BWT peer projection invalid");
  }
  const std::size_t begin = peer.begin + offset;
  return BwtByteIndex::SearchRange{begin, begin + match_count};
}

BidirectionalBwtState BidirectionalBwtByteIndex::extend_left(
    const BidirectionalBwtState& state, std::uint8_t value) const {
  validate_state(state);
  const BwtByteIndex::SearchRange forward_range{state.forward_begin_,
                                                 state.forward_end_};
  const BwtByteIndex::SearchRange reverse_range{state.reverse_begin_,
                                                 state.reverse_end_};
  const BwtByteIndex::SearchRange next_forward =
      forward_.extend(forward_range, value);
  const std::size_t match_count = next_forward.end - next_forward.begin;
  const std::size_t offset = extension_offset(forward_, forward_range, value);
  const BwtByteIndex::SearchRange next_reverse =
      project_peer_interval(reverse_range, offset, match_count);
  return BidirectionalBwtState{next_forward.begin, next_forward.end,
                               next_reverse.begin, next_reverse.end};
}

BidirectionalBwtState BidirectionalBwtByteIndex::extend_right(
    const BidirectionalBwtState& state, std::uint8_t value) const {
  validate_state(state);
  const BwtByteIndex::SearchRange forward_range{state.forward_begin_,
                                                 state.forward_end_};
  const BwtByteIndex::SearchRange reverse_range{state.reverse_begin_,
                                                 state.reverse_end_};
  const BwtByteIndex::SearchRange next_reverse =
      reverse_.extend(reverse_range, value);
  const std::size_t match_count = next_reverse.end - next_reverse.begin;
  const std::size_t offset = extension_offset(reverse_, reverse_range, value);
  const BwtByteIndex::SearchRange next_forward =
      project_peer_interval(forward_range, offset, match_count);
  return BidirectionalBwtState{next_forward.begin, next_forward.end,
                               next_reverse.begin, next_reverse.end};
}

} // namespace algorithms::strings
