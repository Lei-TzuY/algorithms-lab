#include "algorithms/streaming/count_min.hpp"
#include "algorithms/number_theory/modular.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
namespace algorithms::streaming {
CountMinSketch::CountMinSketch(std::size_t width, std::vector<CountMinHashRow> rows)
    : width_(width), rows_(std::move(rows)) {
  const auto prime = hash_prime();
  if (width_ == 0U || static_cast<std::uint64_t>(width_) > prime) {
    throw std::invalid_argument("count-min width must be in [1, hash_prime]");
  }
  if (rows_.empty()) {
    throw std::invalid_argument("count-min requires at least one hash row");
  }
  for (const auto& row : rows_) {
    if (row.multiplier == 0U || row.multiplier >= prime || row.increment >= prime) {
      throw std::invalid_argument("count-min hash coefficient out of range");
    }
  }
  if (rows_.size() > counters_.max_size() / width_) {
    throw std::length_error("count-min table size overflow");
  }
  counters_.assign(rows_.size() * width_, 0U);
}
std::size_t CountMinSketch::bucket(std::size_t row_index, std::uint32_t item) const {
  const auto prime = hash_prime();
  const auto product = algorithms::number_theory::multiply_mod(
      rows_[row_index].multiplier, static_cast<std::uint64_t>(item), prime);
  std::uint64_t field_value = product + rows_[row_index].increment;
  if (field_value >= prime) field_value -= prime;
  const auto column = static_cast<std::size_t>(field_value % static_cast<std::uint64_t>(width_));
  return row_index * width_ + column;
}
void CountMinSketch::add(std::uint32_t item, std::uint64_t amount) {
  if (amount == 0U) return;
  const auto max = std::numeric_limits<std::uint64_t>::max();
  if (total_weight_ > max - amount) throw std::overflow_error("count-min total weight overflow");
  for (std::size_t row = 0; row < rows_.size(); ++row) {
    const auto index = bucket(row, item);
    if (counters_[index] > max - amount) {
      throw std::overflow_error("count-min counter overflow");
    }
  }
  total_weight_ += amount;
  for (std::size_t row = 0; row < rows_.size(); ++row) {
    counters_[bucket(row, item)] += amount;
  }
}
std::uint64_t CountMinSketch::estimate(std::uint32_t item) const {
  std::uint64_t result = std::numeric_limits<std::uint64_t>::max();
  for (std::size_t row = 0; row < rows_.size(); ++row) {
    result = std::min(result, counters_[bucket(row,item)]);
  }
  return result;
}
bool CountMinSketch::valid_state() const noexcept {
  if (width_ == 0U || rows_.empty() || counters_.size() != width_ * rows_.size()) return false;
  for (const auto& row : rows_) {
    if (row.multiplier == 0U || row.multiplier >= hash_prime() || row.increment >= hash_prime()) return false;
  }
  for (const auto count : counters_) if (count > total_weight_) return false;
  return true;
}
}
