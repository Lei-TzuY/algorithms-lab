#include "algorithms/data_structures/run_length_byte_rank.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace algorithms::data_structures {
namespace {

std::size_t checked_add(std::size_t lhs, std::size_t rhs) {
  if (rhs > std::numeric_limits<std::size_t>::max() - lhs) {
    throw std::length_error("run-length byte-rank payload size overflow");
  }
  return lhs + rhs;
}

std::size_t checked_mul(std::size_t lhs, std::size_t rhs) {
  if (lhs != 0U && rhs > std::numeric_limits<std::size_t>::max() / lhs) {
    throw std::length_error("run-length byte-rank payload size overflow");
  }
  return lhs * rhs;
}

}  // namespace

RunLengthByteRankIndex::RunLengthByteRankIndex(
    std::span<const std::uint8_t> values)
    : size_(values.size()) {
  std::array<std::size_t, 256U> totals{};
  std::size_t start = 0U;
  while (start < values.size()) {
    const std::uint8_t value = values[start];
    std::size_t end = start + 1U;
    while (end < values.size() && values[end] == value) {
      ++end;
    }

    const std::size_t run_index = run_starts_.size();
    run_starts_.push_back(start);
    run_values_.push_back(value);
    totals[static_cast<std::size_t>(value)] += end - start;
    run_cumulative_after_.push_back(totals[static_cast<std::size_t>(value)]);
    symbol_run_indices_[static_cast<std::size_t>(value)].push_back(run_index);
    start = end;
  }

  if (run_starts_.size() != run_values_.size() ||
      run_starts_.size() != run_cumulative_after_.size()) {
    throw std::logic_error("run-length byte-rank run-vector invariant violated");
  }

  const std::size_t runs = run_starts_.size();
  std::size_t payload = checked_mul(runs, sizeof(std::size_t));
  payload = checked_add(payload, checked_mul(runs, sizeof(std::uint8_t)));
  payload = checked_add(payload, checked_mul(runs, sizeof(std::size_t)));
  payload = checked_add(payload, checked_mul(runs, sizeof(std::size_t)));
  logical_payload_bytes_ = payload;
}

std::size_t RunLengthByteRankIndex::size() const noexcept { return size_; }

bool RunLengthByteRankIndex::empty() const noexcept { return size_ == 0U; }

std::size_t RunLengthByteRankIndex::run_count() const noexcept {
  return run_starts_.size();
}

std::size_t RunLengthByteRankIndex::run_end(
    std::size_t run_index) const noexcept {
  if (run_index + 1U < run_starts_.size()) {
    return run_starts_[run_index + 1U];
  }
  return size_;
}

std::uint8_t RunLengthByteRankIndex::access(std::size_t index) const {
  if (index >= size_) {
    throw std::out_of_range("run-length byte-rank access index out of range");
  }
  const auto next =
      std::upper_bound(run_starts_.begin(), run_starts_.end(), index);
  const std::size_t run_index =
      static_cast<std::size_t>(std::distance(run_starts_.begin(), next) - 1);
  return run_values_[run_index];
}

std::size_t RunLengthByteRankIndex::rank(std::uint8_t value,
                                         std::size_t end) const {
  if (end > size_) {
    throw std::out_of_range("run-length byte-rank endpoint out of range");
  }
  if (end == 0U) {
    return 0U;
  }

  const auto& indices = symbol_run_indices_[static_cast<std::size_t>(value)];
  const auto first_not_before_end = std::lower_bound(
      indices.begin(), indices.end(), end,
      [this](std::size_t run_index, std::size_t boundary) {
        return run_starts_[run_index] < boundary;
      });
  if (first_not_before_end == indices.begin()) {
    return 0U;
  }

  const std::size_t run_index = *std::prev(first_not_before_end);
  const std::size_t start = run_starts_[run_index];
  const std::size_t end_of_run = run_end(run_index);
  const std::size_t length = end_of_run - start;
  const std::size_t cumulative_after = run_cumulative_after_[run_index];
  if (length > cumulative_after) {
    throw std::logic_error("run-length byte-rank cumulative invariant violated");
  }
  const std::size_t before = cumulative_after - length;
  const std::size_t inside = std::min(end - start, length);
  return before + inside;
}

std::size_t RunLengthByteRankIndex::rank(std::uint8_t value,
                                         std::size_t begin,
                                         std::size_t end) const {
  validate_range(begin, end);
  return rank(value, end) - rank(value, begin);
}

void RunLengthByteRankIndex::validate_range(std::size_t begin,
                                             std::size_t end) const {
  if (begin > end || end > size_) {
    throw std::out_of_range("run-length byte-rank range out of range");
  }
}

std::size_t RunLengthByteRankIndex::logical_payload_bytes() const noexcept {
  return logical_payload_bytes_;
}

}  // namespace algorithms::data_structures
