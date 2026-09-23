#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::randomized {

namespace detail {
__extension__ typedef unsigned __int128 AliasWide;
}  // namespace detail

struct ExactAliasCell {
  std::uint64_t threshold{};
  std::size_t alias{};

  friend bool operator==(const ExactAliasCell&,
                         const ExactAliasCell&) = default;
};

// Exact categorical sampler for non-negative uint64_t weights.
//
// Construction is the integer form of Vose's alias method. For n outcomes with
// total weight W, every table column owns exactly W replay states. In a column,
// threshold states return the column itself and the remaining states return its
// alias. If the column and threshold draws are independent and uniform, outcome
// i is selected by exactly n * weight[i] of the n * W replay states.
//
// sample_from_draws() exposes the bounded replay coordinates directly so the
// table can be verified exhaustively without statistical tests.
//
// sample() converts std::mt19937_64 output into exact bounded uniforms with
// rejection sampling; no modulo-biased reduction is used.
class ExactAliasSampler {
 public:
  explicit ExactAliasSampler(std::vector<std::uint64_t> weights)
      : weights_(std::move(weights)) {
    build();
  }

  [[nodiscard]] std::size_t size() const noexcept {
    return weights_.size();
  }

  [[nodiscard]] std::uint64_t total_weight() const noexcept {
    return total_weight_;
  }

  [[nodiscard]] std::uint64_t weight(
      const std::size_t index) const {
    if (index >= weights_.size()) {
      throw std::out_of_range("alias sampler weight index out of range");
    }
    return weights_[index];
  }

  [[nodiscard]] ExactAliasCell cell(
      const std::size_t index) const {
    if (index >= cells_.size()) {
      throw std::out_of_range("alias sampler cell index out of range");
    }
    return cells_[index];
  }

  [[nodiscard]] std::size_t sample_from_draws(
      const std::size_t column,
      const std::uint64_t threshold_draw) const {
    if (column >= cells_.size()) {
      throw std::out_of_range("alias sampler column out of range");
    }
    if (threshold_draw >= total_weight_) {
      throw std::out_of_range("alias sampler threshold draw out of range");
    }

    const ExactAliasCell selected = cells_[column];
    if (selected.alias >= cells_.size() ||
        selected.threshold > total_weight_) {
      throw std::logic_error("alias sampler table invariant violated");
    }
    return threshold_draw < selected.threshold
               ? column
               : selected.alias;
  }

  [[nodiscard]] std::size_t sample(
      std::mt19937_64& random) const {
    const std::uint64_t column =
        uniform_below(random,
                      static_cast<std::uint64_t>(weights_.size()));
    const std::uint64_t threshold =
        uniform_below(random, total_weight_);
    return sample_from_draws(
        static_cast<std::size_t>(column), threshold);
  }

  // Expensive exact diagnostic. It reconstructs the number of replay states
  // selecting every outcome and compares that mass to n * weight[i].
  [[nodiscard]] bool valid_distribution() const {
    if (weights_.empty() || cells_.size() != weights_.size() ||
        total_weight_ == 0U) {
      return false;
    }

    using Wide = detail::AliasWide;
    const Wide n =
        static_cast<Wide>(weights_.size());
    const Wide total =
        static_cast<Wide>(total_weight_);

    std::vector<Wide> mass(weights_.size(), Wide{0});
    for (std::size_t column = 0U;
         column < cells_.size(); ++column) {
      const ExactAliasCell current = cells_[column];
      if (current.alias >= cells_.size() ||
          current.threshold > total_weight_) {
        return false;
      }

      mass[column] += static_cast<Wide>(current.threshold);
      mass[current.alias] +=
          total - static_cast<Wide>(current.threshold);
    }

    for (std::size_t index = 0U;
         index < weights_.size(); ++index) {
      if (mass[index] !=
          n * static_cast<Wide>(weights_[index])) {
        return false;
      }
    }
    return true;
  }

 private:
  std::vector<std::uint64_t> weights_;
  std::vector<ExactAliasCell> cells_;
  std::uint64_t total_weight_{};

  static std::uint64_t uniform_below(
      std::mt19937_64& random,
      const std::uint64_t bound) {
    if (bound == 0U) {
      throw std::logic_error(
          "alias sampler bounded draw requires positive bound");
    }

    const std::uint64_t rejection =
        (std::uint64_t{0} - bound) % bound;
    for (;;) {
      const std::uint64_t draw = random();
      if (draw >= rejection) {
        return draw % bound;
      }
    }
  }

  void build() {
    if (weights_.empty()) {
      throw std::invalid_argument(
          "alias sampler requires at least one outcome");
    }

    if constexpr (
        sizeof(std::size_t) > sizeof(std::uint64_t)) {
      if (weights_.size() >
          static_cast<std::size_t>(
              std::numeric_limits<std::uint64_t>::max())) {
        throw std::length_error(
            "alias sampler outcome count exceeds uint64_t range");
      }
    }

    using Wide = detail::AliasWide;
    Wide total = 0U;
    for (const std::uint64_t weight : weights_) {
      total += static_cast<Wide>(weight);
      if (total >
          static_cast<Wide>(
              std::numeric_limits<std::uint64_t>::max())) {
        throw std::overflow_error(
            "alias sampler total weight exceeds uint64_t range");
      }
    }

    if (total == 0U) {
      throw std::invalid_argument(
          "alias sampler requires positive total weight");
    }
    total_weight_ = static_cast<std::uint64_t>(total);

    const std::uint64_t count =
        static_cast<std::uint64_t>(weights_.size());
    std::vector<Wide> scaled(weights_.size(), 0U);
    std::vector<std::size_t> small;
    std::vector<std::size_t> large;
    small.reserve(weights_.size());
    large.reserve(weights_.size());

    for (std::size_t index = 0U;
         index < weights_.size(); ++index) {
      scaled[index] =
          static_cast<Wide>(weights_[index]) *
          static_cast<Wide>(count);
      if (scaled[index] < total) {
        small.push_back(index);
      } else {
        large.push_back(index);
      }
    }

    cells_.assign(weights_.size(), ExactAliasCell{});

    while (!small.empty() && !large.empty()) {
      const std::size_t low = small.back();
      small.pop_back();
      const std::size_t high = large.back();
      large.pop_back();

      if (scaled[low] >= total) {
        throw std::logic_error(
            "alias sampler small-list invariant violated");
      }
      cells_[low] = ExactAliasCell{
          static_cast<std::uint64_t>(scaled[low]), high};

      scaled[high] =
          scaled[high] + scaled[low] - total;
      if (scaled[high] < total) {
        small.push_back(high);
      } else {
        large.push_back(high);
      }
    }

    const auto finalize =
        [&](const std::vector<std::size_t>& remaining) {
          for (const std::size_t index : remaining) {
            if (scaled[index] != total) {
              throw std::logic_error(
                  "alias sampler residual mass invariant violated");
            }
            cells_[index] =
                ExactAliasCell{total_weight_, index};
          }
        };

    finalize(small);
    finalize(large);

    if (!valid_distribution()) {
      throw std::logic_error(
          "alias sampler failed exact distribution replay");
    }
  }
};

}  // namespace algorithms::randomized
