#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::combinatorial {

using SymmetricSubmodularOracle =
    std::function<std::int64_t(const std::vector<std::size_t>&)>;

struct SymmetricSubmodularMinimizationResult {
  std::vector<std::size_t> subset;
  std::int64_t value{};
  std::size_t oracle_calls{};
  std::size_t phases{};
  std::size_t key_evaluations{};

  friend bool operator==(const SymmetricSubmodularMinimizationResult&,
                         const SymmetricSubmodularMinimizationResult&) = default;
};

namespace symmetric_submodular_detail {

struct ExactSignedDifference {
  bool negative{};
  std::uint64_t magnitude{};
};

[[nodiscard]] inline ExactSignedDifference exact_difference(
    std::int64_t first, std::int64_t second) noexcept {
  if (first >= second) {
    return ExactSignedDifference{
        false, static_cast<std::uint64_t>(first) -
                   static_cast<std::uint64_t>(second)};
  }
  return ExactSignedDifference{
      true, static_cast<std::uint64_t>(second) -
                static_cast<std::uint64_t>(first)};
}

[[nodiscard]] inline bool less_difference(const ExactSignedDifference& first,
                                          const ExactSignedDifference& second) noexcept {
  if (first.negative != second.negative) {
    return first.negative;
  }
  if (first.negative) {
    return first.magnitude > second.magnitude;
  }
  return first.magnitude < second.magnitude;
}

class OracleEvaluator {
 public:
  explicit OracleEvaluator(const SymmetricSubmodularOracle& oracle)
      : oracle_(oracle) {}

  [[nodiscard]] std::int64_t evaluate(
      const std::vector<std::size_t>& subset) {
    if (calls_ == std::numeric_limits<std::size_t>::max()) {
      throw std::overflow_error("symmetric-submodular oracle-call count overflow");
    }
    ++calls_;
    return oracle_(subset);
  }

  [[nodiscard]] std::size_t calls() const noexcept { return calls_; }

 private:
  const SymmetricSubmodularOracle& oracle_;
  std::size_t calls_{};
};

[[nodiscard]] inline std::vector<std::size_t> sorted_union(
    const std::vector<std::size_t>& first,
    const std::vector<std::size_t>& second) {
  std::vector<std::size_t> result;
  result.reserve(first.size() + second.size());
  std::set_union(first.begin(), first.end(), second.begin(), second.end(),
                 std::back_inserter(result));
  return result;
}

}  // namespace symmetric_submodular_detail

// Queyranne's algorithm for minimizing a caller-supplied symmetric submodular
// set function over non-empty proper subsets of [0, ground_size).
//
// Symmetry and submodularity are caller preconditions. The implementation does
// not pretend to certify those axioms from finitely many oracle calls.
[[nodiscard]] inline SymmetricSubmodularMinimizationResult
minimize_symmetric_submodular_function(
    std::size_t ground_size, const SymmetricSubmodularOracle& oracle) {
  if (ground_size < 2U) {
    throw std::invalid_argument(
        "symmetric-submodular minimization needs at least two elements");
  }
  if (!oracle) {
    throw std::invalid_argument("symmetric-submodular oracle is empty");
  }

  std::vector<std::vector<std::size_t>> supernodes;
  supernodes.reserve(ground_size);
  for (std::size_t element = 0; element < ground_size; ++element) {
    supernodes.push_back(std::vector<std::size_t>{element});
  }

  symmetric_submodular_detail::OracleEvaluator evaluator(oracle);
  SymmetricSubmodularMinimizationResult result;
  bool have_best = false;

  while (supernodes.size() > 1U) {
    if (result.phases == std::numeric_limits<std::size_t>::max()) {
      throw std::overflow_error("symmetric-submodular phase count overflow");
    }
    ++result.phases;

    const std::size_t active_count = supernodes.size();
    std::vector<bool> chosen(active_count, false);
    std::vector<std::size_t> order;
    order.reserve(active_count);
    std::vector<std::size_t> accumulated;

    std::size_t current = 0U;
    chosen[current] = true;
    order.push_back(current);
    accumulated = supernodes[current];

    while (order.size() < active_count) {
      bool found = false;
      std::size_t best_index = 0U;
      symmetric_submodular_detail::ExactSignedDifference best_key;

      for (std::size_t candidate = 0; candidate < active_count; ++candidate) {
        if (chosen[candidate]) {
          continue;
        }
        if (result.key_evaluations == std::numeric_limits<std::size_t>::max()) {
          throw std::overflow_error(
              "symmetric-submodular key-evaluation count overflow");
        }
        ++result.key_evaluations;
        const std::vector<std::size_t> combined =
            symmetric_submodular_detail::sorted_union(
                accumulated, supernodes[candidate]);
        const std::int64_t combined_value = evaluator.evaluate(combined);
        const std::int64_t candidate_value = evaluator.evaluate(supernodes[candidate]);
        const auto key = symmetric_submodular_detail::exact_difference(
            combined_value, candidate_value);

        if (!found || symmetric_submodular_detail::less_difference(key, best_key)) {
          found = true;
          best_index = candidate;
          best_key = key;
        }
      }

      chosen[best_index] = true;
      order.push_back(best_index);
      accumulated = symmetric_submodular_detail::sorted_union(
          accumulated, supernodes[best_index]);
    }

    const std::size_t second_last = order[order.size() - 2U];
    const std::size_t last = order.back();
    const std::int64_t candidate_value = evaluator.evaluate(supernodes[last]);
    if (!have_best || candidate_value < result.value) {
      have_best = true;
      result.value = candidate_value;
      result.subset = supernodes[last];
    }

    std::vector<std::size_t> merged =
        symmetric_submodular_detail::sorted_union(supernodes[second_last],
                                                  supernodes[last]);
    supernodes[second_last] = std::move(merged);
    supernodes.erase(supernodes.begin() + static_cast<std::ptrdiff_t>(last));
  }

  result.oracle_calls = evaluator.calls();
  return result;
}

}  // namespace algorithms::combinatorial
