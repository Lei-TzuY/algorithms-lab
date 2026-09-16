#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::streaming {

struct KllWeightedSample {
  std::int64_t value{};
  std::uint64_t weight{};
  std::size_t level{};

  friend bool operator==(const KllWeightedSample&, const KllWeightedSample&) =
      default;
};

struct KllQuantileEstimate {
  std::int64_t value{};
  std::uint64_t target_rank{};
  std::uint64_t deterministic_rank_error_bound{};

  friend bool operator==(const KllQuantileEstimate&,
                         const KllQuantileEstimate&) = default;
};

// Direct varying-capacity compactor hierarchy from the mergeable KLL core.
// This educational baseline deliberately does not fold capacity-two bottom
// levels into an O(1)-space sampler and does not expose calibrated epsilon/delta
// confidence constants. The caller-facing capacity parameter k controls the
// top compactor; lower capacities decrease by a 2/3 geometric recurrence.
class KllQuantileSketch {
 public:
  explicit KllQuantileSketch(std::size_t k, std::uint64_t seed)
      : k_(k), seed_(seed), rng_state_(seed), levels_(1U) {
    if (k_ < 2U || k_ == std::numeric_limits<std::size_t>::max()) {
      throw std::invalid_argument("KLL k must be in [2, SIZE_MAX-1]");
    }
  }

  void insert(std::int64_t value) {
    if (count_ == std::numeric_limits<std::uint64_t>::max()) {
      throw std::length_error("KLL stream count overflow");
    }
    levels_.front().push_back(value);
    ++count_;
    rebalance();
  }

  [[nodiscard]] std::size_t k() const noexcept { return k_; }
  [[nodiscard]] std::uint64_t seed() const noexcept { return seed_; }
  [[nodiscard]] std::uint64_t count() const noexcept { return count_; }
  [[nodiscard]] std::size_t level_count() const noexcept {
    return levels_.size();
  }
  [[nodiscard]] std::uint64_t compaction_count() const noexcept {
    return compaction_count_;
  }
  [[nodiscard]] std::uint64_t deterministic_rank_error_bound() const noexcept {
    return deterministic_rank_error_bound_;
  }

  [[nodiscard]] std::size_t retained_count() const noexcept {
    std::size_t total = 0U;
    for (const auto& level : levels_) {
      if (level.size() > std::numeric_limits<std::size_t>::max() - total) {
        return std::numeric_limits<std::size_t>::max();
      }
      total += level.size();
    }
    return total;
  }

  [[nodiscard]] std::size_t level_capacity(std::size_t level) const {
    if (level >= levels_.size()) {
      throw std::out_of_range("KLL level out of range");
    }
    std::size_t scaled = k_;
    const std::size_t distance = levels_.size() - 1U - level;
    for (std::size_t step = 0U; step < distance; ++step) {
      // ceil(2*scaled/3) without overflowing 2*scaled.
      scaled = 2U * (scaled / 3U) + (scaled % 3U);
    }
    return scaled + 1U;
  }

  [[nodiscard]] const std::vector<std::vector<std::int64_t>>& levels()
      const noexcept {
    return levels_;
  }

  [[nodiscard]] std::vector<KllWeightedSample> weighted_samples() const {
    std::vector<KllWeightedSample> samples;
    samples.reserve(retained_count());
    for (std::size_t level = 0U; level < levels_.size(); ++level) {
      const std::uint64_t weight = level_weight(level);
      for (const std::int64_t value : levels_[level]) {
        samples.push_back(KllWeightedSample{value, weight, level});
      }
    }
    std::sort(samples.begin(), samples.end(),
              [](const KllWeightedSample& left,
                 const KllWeightedSample& right) {
                if (left.value != right.value) {
                  return left.value < right.value;
                }
                return left.level < right.level;
              });
    return samples;
  }

  [[nodiscard]] std::uint64_t estimated_rank(std::int64_t value) const {
    std::uint64_t rank = 0U;
    for (std::size_t level = 0U; level < levels_.size(); ++level) {
      const std::uint64_t weight = level_weight(level);
      for (const std::int64_t sample : levels_[level]) {
        if (sample <= value) {
          if (weight > count_ || rank > count_ - weight) {
            throw std::logic_error("KLL retained weight exceeds stream count");
          }
          rank += weight;
        }
      }
    }
    return rank;
  }

  [[nodiscard]] std::optional<KllQuantileEstimate> query_rank(
      std::uint64_t one_based_rank) const {
    if (count_ == 0U) {
      return std::nullopt;
    }
    if (one_based_rank == 0U || one_based_rank > count_) {
      throw std::out_of_range("KLL query rank is outside [1,count]");
    }

    const auto samples = weighted_samples();
    std::uint64_t cumulative = 0U;
    for (const KllWeightedSample& sample : samples) {
      if (sample.weight > count_ || cumulative > count_ - sample.weight) {
        throw std::logic_error("KLL retained weight exceeds stream count");
      }
      cumulative += sample.weight;
      if (cumulative >= one_based_rank) {
        return KllQuantileEstimate{sample.value, one_based_rank,
                                   deterministic_rank_error_bound_};
      }
    }
    throw std::logic_error("KLL retained mass does not cover stream count");
  }

  [[nodiscard]] bool valid_state() const noexcept {
    if (levels_.empty()) {
      return false;
    }
    if (count_ == 0U) {
      return levels_.size() == 1U && levels_.front().empty() &&
             compaction_count_ == 0U &&
             deterministic_rank_error_bound_ == 0U;
    }
    if (levels_.back().empty()) {
      return false;
    }
    if (deterministic_rank_error_bound_ > count_) {
      return false;
    }

    std::uint64_t retained_mass = 0U;
    for (std::size_t level = 0U; level < levels_.size(); ++level) {
      if (levels_[level].size() >= capacity_unchecked(level)) {
        return false;
      }
      if (level >= std::numeric_limits<std::uint64_t>::digits) {
        return false;
      }
      const std::uint64_t weight = std::uint64_t{1U} << level;
      const std::uint64_t size =
          static_cast<std::uint64_t>(levels_[level].size());
      if (size != 0U && weight > count_ / size) {
        return false;
      }
      const std::uint64_t mass = weight * size;
      if (retained_mass > count_ - mass) {
        return false;
      }
      retained_mass += mass;
    }
    return retained_mass == count_;
  }

 private:
  [[nodiscard]] std::size_t capacity_unchecked(std::size_t level) const
      noexcept {
    std::size_t scaled = k_;
    const std::size_t distance = levels_.size() - 1U - level;
    for (std::size_t step = 0U; step < distance; ++step) {
      scaled = 2U * (scaled / 3U) + (scaled % 3U);
    }
    return scaled + 1U;
  }

  [[nodiscard]] std::uint64_t level_weight(std::size_t level) const {
    if (level >= std::numeric_limits<std::uint64_t>::digits) {
      throw std::length_error("KLL level weight exceeds uint64_t");
    }
    return std::uint64_t{1U} << level;
  }

  [[nodiscard]] std::uint64_t next_random() noexcept {
    rng_state_ += 0x9E3779B97F4A7C15ULL;
    std::uint64_t value = rng_state_;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
  }

  void update_error_bound(std::size_t level) {
    const std::uint64_t weight = level_weight(level);
    const std::uint64_t remaining = count_ - deterministic_rank_error_bound_;
    if (weight >= remaining) {
      deterministic_rank_error_bound_ = count_;
    } else {
      deterministic_rank_error_bound_ += weight;
    }
  }

  void compact(std::size_t level) {
    if (level >= levels_.size() || levels_[level].size() < 2U) {
      throw std::logic_error("KLL invalid compaction request");
    }

    if (level + 1U == levels_.size()) {
      if (levels_.size() >= std::numeric_limits<std::uint64_t>::digits) {
        throw std::length_error("KLL hierarchy exceeds uint64_t weight domain");
      }
      levels_.emplace_back();
    }

    auto& current = levels_[level];
    std::sort(current.begin(), current.end());
    const bool odd = (current.size() % 2U) != 0U;
    const std::optional<std::int64_t> retained =
        odd ? std::optional<std::int64_t>(current.back()) : std::nullopt;
    const std::size_t compacted_size = current.size() - (odd ? 1U : 0U);
    const std::size_t promoted_count = compacted_size / 2U;

    std::vector<std::int64_t> promoted;
    promoted.reserve(promoted_count);
    const std::size_t parity = static_cast<std::size_t>(next_random() & 1ULL);
    for (std::size_t index = parity; index < compacted_size; index += 2U) {
      promoted.push_back(current[index]);
    }
    if (promoted.size() != promoted_count) {
      throw std::logic_error("KLL compaction produced wrong promotion count");
    }

    auto& next = levels_[level + 1U];
    if (promoted.size() > next.max_size() - next.size()) {
      throw std::length_error("KLL next level size overflow");
    }
    next.reserve(next.size() + promoted.size());
    next.insert(next.end(), promoted.begin(), promoted.end());

    current.clear();
    if (retained.has_value()) {
      current.push_back(*retained);
    }
    ++compaction_count_;
    update_error_bound(level);
  }

  void rebalance() {
    for (;;) {
      bool compacted = false;
      for (std::size_t level = 0U; level < levels_.size(); ++level) {
        if (levels_[level].size() >= capacity_unchecked(level)) {
          compact(level);
          compacted = true;
          break;  // H may have changed; recompute every capacity.
        }
      }
      if (!compacted) {
        break;
      }
    }
  }

  std::size_t k_;
  std::uint64_t seed_;
  std::uint64_t rng_state_;
  std::uint64_t count_{};
  std::uint64_t compaction_count_{};
  std::uint64_t deterministic_rank_error_bound_{};
  std::vector<std::vector<std::int64_t>> levels_;
};

}  // namespace algorithms::streaming
