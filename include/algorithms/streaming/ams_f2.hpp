#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::streaming {

struct AmsF2SampleState {
  std::uint64_t position{};
  std::optional<std::uint32_t> item;
  std::uint64_t suffix_count{};

  friend bool operator==(const AmsF2SampleState&, const AmsF2SampleState&) = default;
};

// Fixed-horizon Alon-Matias-Szegedy second-frequency-moment sketch using the
// sampled-position estimator. Explicit sample positions define the theorem
// boundary. ams_replayable_sample_positions() is only a deterministic replay
// helper; it does not turn a finite seeded run into an exact or confidence-
// calibrated estimator.
class AmsF2FixedHorizonSketch {
 public:
  AmsF2FixedHorizonSketch(std::uint64_t expected_length,
                          std::vector<std::uint64_t> sample_positions)
      : expected_length_(expected_length) {
    if (expected_length_ == 0U) {
      if (!sample_positions.empty()) {
        throw std::invalid_argument(
            "empty AMS horizon cannot have sample positions");
      }
      return;
    }
    if (sample_positions.empty()) {
      throw std::invalid_argument(
          "non-empty AMS horizon requires at least one row");
    }

    samples_.reserve(sample_positions.size());
    for (const std::uint64_t position : sample_positions) {
      if (position >= expected_length_) {
        throw std::out_of_range("AMS sample position outside stream horizon");
      }
      samples_.push_back(AmsF2SampleState{position, std::nullopt, 0U});
    }
  }

  void add(std::uint32_t item) {
    if (processed_length_ >= expected_length_) {
      throw std::length_error("AMS stream exceeded fixed horizon");
    }

    for (AmsF2SampleState& sample : samples_) {
      if (sample.position == processed_length_) {
        sample.item = item;
        sample.suffix_count = 1U;
      } else if (sample.position < processed_length_ &&
                 sample.item.has_value() && *sample.item == item) {
        ++sample.suffix_count;
      }
    }
    ++processed_length_;
  }

  [[nodiscard]] std::uint64_t expected_length() const noexcept {
    return expected_length_;
  }
  [[nodiscard]] std::uint64_t processed_length() const noexcept {
    return processed_length_;
  }
  [[nodiscard]] std::size_t row_count() const noexcept { return samples_.size(); }
  [[nodiscard]] bool complete() const noexcept {
    return processed_length_ == expected_length_;
  }
  [[nodiscard]] const std::vector<AmsF2SampleState>& samples() const noexcept {
    return samples_;
  }

  [[nodiscard]] std::vector<long double> row_estimates() const {
    if (!complete()) {
      throw std::logic_error(
          "AMS estimate requested before fixed horizon completed");
    }
    if (expected_length_ == 0U) {
      return {};
    }

    std::vector<long double> estimates;
    estimates.reserve(samples_.size());
    const long double horizon = static_cast<long double>(expected_length_);
    for (const AmsF2SampleState& sample : samples_) {
      if (!sample.item.has_value() || sample.suffix_count == 0U) {
        throw std::logic_error("completed AMS row lacks sampled suffix state");
      }
      const long double suffix = static_cast<long double>(sample.suffix_count);
      estimates.push_back(horizon * ((2.0L * suffix) - 1.0L));
    }
    return estimates;
  }

  [[nodiscard]] long double estimate_f2() const {
    if (!complete()) {
      throw std::logic_error(
          "AMS estimate requested before fixed horizon completed");
    }
    if (expected_length_ == 0U) {
      return 0.0L;
    }

    const std::vector<long double> estimates = row_estimates();
    long double total = 0.0L;
    for (const long double estimate : estimates) {
      total += estimate;
    }
    return total / static_cast<long double>(estimates.size());
  }

  [[nodiscard]] bool valid_state() const noexcept {
    if (processed_length_ > expected_length_) {
      return false;
    }
    if (expected_length_ == 0U) {
      return samples_.empty() && processed_length_ == 0U;
    }
    if (samples_.empty()) {
      return false;
    }

    for (const AmsF2SampleState& sample : samples_) {
      if (sample.position >= expected_length_) {
        return false;
      }
      if (processed_length_ <= sample.position) {
        if (sample.item.has_value() || sample.suffix_count != 0U) {
          return false;
        }
        continue;
      }
      if (!sample.item.has_value() || sample.suffix_count == 0U) {
        return false;
      }
      const std::uint64_t observed_suffix_length =
          processed_length_ - sample.position;
      if (sample.suffix_count > observed_suffix_length) {
        return false;
      }
    }
    return true;
  }

 private:
  std::uint64_t expected_length_{};
  std::uint64_t processed_length_{};
  std::vector<AmsF2SampleState> samples_;
};

namespace detail {

inline std::uint64_t ams_splitmix64_next(std::uint64_t& state) noexcept {
  state += 0x9e3779b97f4a7c15ULL;
  std::uint64_t value = state;
  value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
  value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
  return value ^ (value >> 31U);
}

inline std::uint64_t ams_bounded_replay_word(std::uint64_t& state,
                                             std::uint64_t bound) noexcept {
  const std::uint64_t threshold = (std::uint64_t{0} - bound) % bound;
  while (true) {
    const std::uint64_t value = ams_splitmix64_next(state);
    if (value >= threshold) {
      return value % bound;
    }
  }
}

}  // namespace detail

// Deterministic, non-cryptographic replay helper with an explicitly specified
// SplitMix64 stream and rejection sampler. The AMS expectation theorem applies
// to uniform sample positions; no finite-row confidence theorem is claimed for
// one deterministic seed by this API.
[[nodiscard]] inline std::vector<std::uint64_t> ams_replayable_sample_positions(
    std::uint64_t expected_length, std::size_t row_count, std::uint64_t seed) {
  if (expected_length == 0U) {
    if (row_count != 0U) {
      throw std::invalid_argument(
          "cannot sample a nonzero AMS row count from empty horizon");
    }
    return {};
  }
  if (row_count == 0U) {
    throw std::invalid_argument(
        "non-empty AMS horizon requires at least one row");
  }

  std::vector<std::uint64_t> positions;
  positions.reserve(row_count);
  std::uint64_t state = seed;
  for (std::size_t row = 0; row < row_count; ++row) {
    static_cast<void>(row);
    positions.push_back(
        detail::ams_bounded_replay_word(state, expected_length));
  }
  return positions;
}

}  // namespace algorithms::streaming
