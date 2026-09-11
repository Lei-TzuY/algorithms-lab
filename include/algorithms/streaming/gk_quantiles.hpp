#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace algorithms::streaming {

struct GkTuple {
  std::int64_t value{};
  std::size_t g{};
  std::size_t delta{};

  friend bool operator==(const GkTuple&, const GkTuple&) = default;
};

struct GkRankEstimate {
  std::int64_t value{};
  std::size_t target_rank{};
  std::size_t rank_error_bound{};

  friend bool operator==(const GkRankEstimate&, const GkRankEstimate&) = default;
};

class GreenwaldKhannaSummary {
 public:
  explicit GreenwaldKhannaSummary(std::size_t error_denominator)
      : error_denominator_(error_denominator) {
    if (error_denominator_ < 2U) {
      throw std::invalid_argument("GK error denominator must be at least two");
    }
  }

  void insert(std::int64_t value) {
    if (count_ == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error("GK stream count overflow");
    }

    const auto position = std::upper_bound(
        tuples_.begin(), tuples_.end(), value,
        [](std::int64_t needle, const GkTuple& tuple) {
          return needle < tuple.value;
        });
    const std::size_t index =
        static_cast<std::size_t>(position - tuples_.begin());

    std::size_t delta = 0U;
    if (index != 0U && index != tuples_.size()) {
      const GkTuple& next = tuples_[index];
      if (next.g == 0U ||
          next.delta > std::numeric_limits<std::size_t>::max() - next.g) {
        throw std::logic_error("GK summary invariant is corrupted");
      }
      delta = next.g + next.delta - 1U;
    }

    tuples_.insert(position, GkTuple{value, 1U, delta});
    ++count_;
    compress();
  }

  [[nodiscard]] std::size_t count() const noexcept { return count_; }
  [[nodiscard]] std::size_t summary_size() const noexcept {
    return tuples_.size();
  }
  [[nodiscard]] std::size_t error_denominator() const noexcept {
    return error_denominator_;
  }

  [[nodiscard]] std::size_t rank_error_bound() const noexcept {
    if (count_ == 0U) {
      return 0U;
    }
    return count_ / error_denominator_ +
           (count_ % error_denominator_ == 0U ? 0U : 1U);
  }

  [[nodiscard]] const std::vector<GkTuple>& tuples() const noexcept {
    return tuples_;
  }

  [[nodiscard]] std::optional<GkRankEstimate> query_rank(
      std::size_t one_based_rank) const {
    if (count_ == 0U) {
      return std::nullopt;
    }
    if (one_based_rank == 0U || one_based_rank > count_) {
      throw std::out_of_range("GK query rank is outside [1,count]");
    }

    const std::size_t error = rank_error_bound();
    const std::size_t lower =
        one_based_rank > error ? one_based_rank - error : 1U;
    const std::size_t upper =
        one_based_rank > std::numeric_limits<std::size_t>::max() - error
            ? std::numeric_limits<std::size_t>::max()
            : one_based_rank + error;

    std::size_t min_rank = 0U;
    for (const GkTuple& tuple : tuples_) {
      if (min_rank > count_ || tuple.g > count_ - min_rank) {
        throw std::logic_error("GK summary invariant is corrupted");
      }
      min_rank += tuple.g;
      if (tuple.delta > count_ - min_rank) {
        throw std::logic_error("GK summary invariant is corrupted");
      }
      const std::size_t max_rank = min_rank + tuple.delta;
      if (min_rank >= lower && max_rank <= upper) {
        return GkRankEstimate{tuple.value, one_based_rank, error};
      }
    }

    throw std::logic_error("GK rank invariant failed to answer a query");
  }

  [[nodiscard]] bool valid_state() const noexcept {
    if (count_ == 0U) {
      return tuples_.empty();
    }
    if (tuples_.empty()) {
      return false;
    }
    if (tuples_.front().delta != 0U || tuples_.back().delta != 0U) {
      return false;
    }

    const std::size_t allowed = allowed_gap();
    std::size_t prefix = 0U;
    for (std::size_t i = 0U; i < tuples_.size(); ++i) {
      const GkTuple& tuple = tuples_[i];
      if (tuple.g == 0U) {
        return false;
      }
      if (i != 0U && tuples_[i - 1U].value > tuple.value) {
        return false;
      }
      if (tuple.g > allowed || tuple.delta > allowed - tuple.g) {
        return false;
      }
      if (prefix > count_ || tuple.g > count_ - prefix) {
        return false;
      }
      prefix += tuple.g;
      if (tuple.delta > count_ - prefix) {
        return false;
      }
    }
    return prefix == count_;
  }

 private:
  [[nodiscard]] std::size_t allowed_gap() const noexcept {
    const std::size_t error = rank_error_bound();
    if (error > std::numeric_limits<std::size_t>::max() / 2U) {
      return std::numeric_limits<std::size_t>::max();
    }
    return error * 2U;
  }

  void compress() {
    if (tuples_.size() <= 2U) {
      return;
    }
    const std::size_t allowed = allowed_gap();
    std::size_t index = tuples_.size() - 2U;
    while (index >= 1U) {
      GkTuple& left = tuples_[index];
      GkTuple& right = tuples_[index + 1U];
      if (right.g <= allowed && right.delta <= allowed - right.g) {
        const std::size_t remaining = allowed - right.g - right.delta;
        if (left.g <= remaining) {
          right.g += left.g;
          tuples_.erase(tuples_.begin() +
                        static_cast<std::ptrdiff_t>(index));
        }
      }
      if (index == 1U) {
        break;
      }
      --index;
    }
  }

  std::size_t error_denominator_;
  std::size_t count_{};
  std::vector<GkTuple> tuples_;
};

}  // namespace algorithms::streaming
