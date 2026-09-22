#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

struct StaticRangeModeResult {
  std::int64_t value{};
  std::size_t frequency{};

  friend bool operator==(const StaticRangeModeResult&,
                         const StaticRangeModeResult&) = default;
};

// Exact static range-mode index over signed 64-bit values.
//
// Query ranges are half-open [begin,end). Nonempty queries return the value with
// maximum frequency; ties choose the numerically smaller value.
//
// Construction coordinate-compresses values, builds occurrence-position lists,
// and precomputes the exact mode for every interval of complete sqrt-sized
// blocks. A query verifies the complete-block mode plus every value occurring
// in the two boundary fringes using exact occurrence counts.
class StaticRangeMode {
 public:
  using Value = std::int64_t;

  explicit StaticRangeMode(std::span<const Value> values)
      : size_(values.size()) {
    unique_values_.assign(values.begin(), values.end());
    std::sort(unique_values_.begin(), unique_values_.end());
    unique_values_.erase(
        std::unique(unique_values_.begin(), unique_values_.end()),
        unique_values_.end());

    compressed_.reserve(size_);
    positions_.resize(unique_values_.size());
    for (std::size_t index = 0U; index < size_; ++index) {
      const auto it =
          std::lower_bound(unique_values_.begin(), unique_values_.end(),
                           values[index]);
      const std::size_t id =
          static_cast<std::size_t>(it - unique_values_.begin());
      compressed_.push_back(id);
      positions_[id].push_back(index);
    }

    choose_block_shape();
    build_core_modes();
  }

  explicit StaticRangeMode(const std::vector<Value>& values)
      : StaticRangeMode(std::span<const Value>(values)) {}

  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] bool empty() const noexcept { return size_ == 0U; }
  [[nodiscard]] std::size_t distinct_value_count() const noexcept {
    return unique_values_.size();
  }
  [[nodiscard]] std::size_t block_size() const noexcept {
    return block_size_;
  }
  [[nodiscard]] std::size_t block_count() const noexcept {
    return block_count_;
  }

  [[nodiscard]] std::size_t frequency(
      const Value value, const std::size_t begin,
      const std::size_t end) const {
    validate_range(begin, end);
    const auto it =
        std::lower_bound(unique_values_.begin(), unique_values_.end(), value);
    if (it == unique_values_.end() || *it != value) {
      return 0U;
    }
    const std::size_t id =
        static_cast<std::size_t>(it - unique_values_.begin());
    return count_id(id, begin, end);
  }

  [[nodiscard]] std::optional<StaticRangeModeResult> query(
      const std::size_t begin, const std::size_t end) const {
    validate_range(begin, end);
    if (begin == end) {
      return std::nullopt;
    }

    const std::size_t first_full =
        begin / block_size_ + ((begin % block_size_) != 0U ? 1U : 0U);
    const std::size_t past_last_full = end / block_size_;
    const bool has_full_core = first_full < past_last_full;

    std::vector<std::size_t> candidates;
    if (has_full_core) {
      candidates.reserve(1U + 2U * block_size_);
      candidates.push_back(
          core_mode(first_full, past_last_full - 1U));

      const std::size_t left_end = first_full * block_size_;
      for (std::size_t index = begin; index < left_end; ++index) {
        candidates.push_back(compressed_[index]);
      }

      const std::size_t right_begin = past_last_full * block_size_;
      for (std::size_t index = right_begin; index < end; ++index) {
        candidates.push_back(compressed_[index]);
      }
    } else {
      candidates.reserve(end - begin);
      for (std::size_t index = begin; index < end; ++index) {
        candidates.push_back(compressed_[index]);
      }
    }

    std::sort(candidates.begin(), candidates.end());
    candidates.erase(
        std::unique(candidates.begin(), candidates.end()),
        candidates.end());

    std::size_t best_id = kNoId;
    std::size_t best_frequency = 0U;
    for (const std::size_t id : candidates) {
      const std::size_t exact = count_id(id, begin, end);
      if (best_id == kNoId || exact > best_frequency ||
          (exact == best_frequency && id < best_id)) {
        best_id = id;
        best_frequency = exact;
      }
    }

    if (best_id == kNoId || best_frequency == 0U) {
      throw std::logic_error("static range-mode candidate set is empty");
    }
    return StaticRangeModeResult{unique_values_[best_id], best_frequency};
  }

 private:
  static constexpr std::size_t kNoId =
      std::numeric_limits<std::size_t>::max();

  std::size_t size_{};
  std::size_t block_size_{1U};
  std::size_t block_count_{};
  std::vector<Value> unique_values_;
  std::vector<std::size_t> compressed_;
  std::vector<std::vector<std::size_t>> positions_;
  std::vector<std::size_t> core_modes_;

  void choose_block_shape() {
    if (size_ == 0U) {
      block_size_ = 1U;
      block_count_ = 0U;
      return;
    }

    const long double root =
        std::sqrt(static_cast<long double>(size_));
    block_size_ =
        std::max<std::size_t>(1U, static_cast<std::size_t>(root));
    block_count_ =
        size_ / block_size_ + ((size_ % block_size_) != 0U ? 1U : 0U);

    if (block_count_ != 0U &&
        block_count_ >
            std::numeric_limits<std::size_t>::max() / block_count_) {
      throw std::length_error("static range-mode block table is too large");
    }
  }

  void build_core_modes() {
    if (block_count_ == 0U) {
      return;
    }

    core_modes_.assign(block_count_ * block_count_, kNoId);
    std::vector<std::size_t> counts(unique_values_.size(), 0U);

    for (std::size_t first = 0U; first < block_count_; ++first) {
      std::fill(counts.begin(), counts.end(), 0U);
      std::size_t best_id = kNoId;
      std::size_t best_frequency = 0U;

      for (std::size_t last = first; last < block_count_; ++last) {
        const std::size_t block_begin = last * block_size_;
        const std::size_t block_end =
            block_begin + std::min(block_size_, size_ - block_begin);

        for (std::size_t index = block_begin; index < block_end; ++index) {
          const std::size_t id = compressed_[index];
          const std::size_t exact = ++counts[id];
          if (best_id == kNoId || exact > best_frequency ||
              (exact == best_frequency && id < best_id)) {
            best_id = id;
            best_frequency = exact;
          }
        }

        core_modes_[first * block_count_ + last] = best_id;
      }
    }
  }

  [[nodiscard]] std::size_t core_mode(
      const std::size_t first, const std::size_t last) const {
    if (first > last || last >= block_count_) {
      throw std::logic_error("static range-mode core block range invalid");
    }
    const std::size_t id =
        core_modes_[first * block_count_ + last];
    if (id == kNoId) {
      throw std::logic_error("static range-mode core has no mode");
    }
    return id;
  }

  [[nodiscard]] std::size_t count_id(
      const std::size_t id, const std::size_t begin,
      const std::size_t end) const {
    const auto& positions = positions_[id];
    const auto first =
        std::lower_bound(positions.begin(), positions.end(), begin);
    const auto last =
        std::lower_bound(first, positions.end(), end);
    return static_cast<std::size_t>(last - first);
  }

  void validate_range(
      const std::size_t begin, const std::size_t end) const {
    if (begin > end || end > size_) {
      throw std::out_of_range("static range-mode range out of range");
    }
  }
};

}  // namespace algorithms::data_structures
