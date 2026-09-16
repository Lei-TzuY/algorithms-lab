#pragma once

#include "algorithms/number_theory/modular.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::combinatorial {

struct LinearMatroidParityPair {
  std::vector<std::uint64_t> first;
  std::vector<std::uint64_t> second;

  friend bool operator==(const LinearMatroidParityPair&,
                         const LinearMatroidParityPair&) = default;
};

struct LinearMatroidParityResult {
  std::vector<std::size_t> selected_pairs;
  std::size_t search_nodes{};
  std::size_t pruned_nodes{};
  std::size_t rank_bound_evaluations{};

  friend bool operator==(const LinearMatroidParityResult&,
                         const LinearMatroidParityResult&) = default;
};

inline constexpr std::size_t kLinearMatroidParityMaxPairs = 20U;
inline constexpr std::size_t kLinearMatroidParityMaxDimension = 128U;

namespace linear_matroid_parity_detail {

[[nodiscard]] inline std::uint64_t subtract_mod(std::uint64_t first,
                                                std::uint64_t second,
                                                std::uint64_t modulus) noexcept {
  return first >= second ? first - second : modulus - (second - first);
}

class IncrementalLinearBasis {
 public:
  IncrementalLinearBasis(std::size_t dimension, std::uint64_t modulus)
      : dimension_(dimension),
        modulus_(modulus),
        pivots_(dimension),
        occupied_(dimension, false) {}

  [[nodiscard]] std::size_t rank() const noexcept { return rank_; }

  [[nodiscard]] bool insert(const std::vector<std::uint64_t>& input) {
    std::vector<std::uint64_t> value = input;
    for (std::uint64_t& coordinate : value) {
      coordinate %= modulus_;
    }

    for (std::size_t pivot = 0; pivot < dimension_; ++pivot) {
      if (value[pivot] == 0U) {
        continue;
      }

      if (occupied_[pivot]) {
        const std::uint64_t factor = value[pivot];
        for (std::size_t column = pivot; column < dimension_; ++column) {
          const std::uint64_t product = number_theory::multiply_mod(
              factor, pivots_[pivot][column], modulus_);
          value[column] = subtract_mod(value[column], product, modulus_);
        }
        continue;
      }

      const std::uint64_t inverse = number_theory::power_mod(
          value[pivot], modulus_ - 2U, modulus_);
      for (std::size_t column = pivot; column < dimension_; ++column) {
        value[column] =
            number_theory::multiply_mod(value[column], inverse, modulus_);
      }
      pivots_[pivot] = std::move(value);
      occupied_[pivot] = true;
      ++rank_;
      return true;
    }
    return false;
  }

 private:
  std::size_t dimension_{};
  std::uint64_t modulus_{};
  std::vector<std::vector<std::uint64_t>> pivots_;
  std::vector<bool> occupied_;
  std::size_t rank_{};
};

class Solver {
 public:
  Solver(std::vector<LinearMatroidParityPair> pairs, std::uint64_t modulus,
         std::size_t dimension)
      : pairs_(std::move(pairs)),
        modulus_(modulus),
        dimension_(dimension) {}

  [[nodiscard]] LinearMatroidParityResult solve() {
    IncrementalLinearBasis basis(dimension_, modulus_);
    std::vector<std::size_t> selected;
    search(0U, basis, selected);
    result_.selected_pairs = std::move(best_);
    return result_;
  }

 private:
  [[nodiscard]] std::size_t additional_pair_rank_bound(
      std::size_t pair_index, const IncrementalLinearBasis& basis) {
    ++result_.rank_bound_evaluations;
    IncrementalLinearBasis potential = basis;
    for (std::size_t index = pair_index; index < pairs_.size(); ++index) {
      static_cast<void>(potential.insert(pairs_[index].first));
      static_cast<void>(potential.insert(pairs_[index].second));
    }
    const std::size_t extra_rank = potential.rank() - basis.rank();
    return extra_rank / 2U;
  }

  void search(std::size_t pair_index, const IncrementalLinearBasis& basis,
              std::vector<std::size_t>& selected) {
    ++result_.search_nodes;
    const std::size_t remaining = pairs_.size() - pair_index;
    if (selected.size() + remaining <= best_.size()) {
      ++result_.pruned_nodes;
      return;
    }

    const std::size_t rank_bound = additional_pair_rank_bound(pair_index, basis);
    if (selected.size() + std::min(remaining, rank_bound) <= best_.size()) {
      ++result_.pruned_nodes;
      return;
    }

    if (pair_index == pairs_.size()) {
      if (selected.size() > best_.size()) {
        best_ = selected;
      }
      return;
    }

    IncrementalLinearBasis included = basis;
    const bool first_independent = included.insert(pairs_[pair_index].first);
    const bool second_independent =
        first_independent && included.insert(pairs_[pair_index].second);
    if (second_independent) {
      selected.push_back(pair_index);
      search(pair_index + 1U, included, selected);
      selected.pop_back();
    }

    search(pair_index + 1U, basis, selected);
  }

  std::vector<LinearMatroidParityPair> pairs_;
  std::uint64_t modulus_{};
  std::size_t dimension_{};
  std::vector<std::size_t> best_;
  LinearMatroidParityResult result_;
};

}  // namespace linear_matroid_parity_detail

// Exact bounded baseline for linear matroid parity over a prime field.
// Ground elements arrive as ordered pairs of vectors. The result selects the
// maximum number of whole pairs whose combined vectors are linearly independent.
//
// This direct educational solver is deliberately exponential in the pair count:
// it explores include/exclude decisions and prunes with a safe rank upper bound.
// It is not the polynomial linear-matroid-parity algorithm of Lovasz/Gabow.
// Pair order determines deterministic search order; no canonical optimum among
// multiple maximum-cardinality solutions is claimed.
[[nodiscard]] inline LinearMatroidParityResult
maximum_cardinality_linear_matroid_parity(
    const std::vector<LinearMatroidParityPair>& input_pairs,
    std::uint64_t modulus) {
  if (!number_theory::is_prime(modulus)) {
    throw std::invalid_argument("linear matroid parity modulus must be prime");
  }
  if (input_pairs.size() > kLinearMatroidParityMaxPairs) {
    throw std::length_error("linear matroid parity pair limit exceeded");
  }

  const std::size_t dimension =
      input_pairs.empty() ? 0U : input_pairs.front().first.size();
  if (dimension > kLinearMatroidParityMaxDimension) {
    throw std::length_error("linear matroid parity dimension limit exceeded");
  }

  std::vector<LinearMatroidParityPair> pairs = input_pairs;
  for (LinearMatroidParityPair& pair : pairs) {
    if (pair.first.size() != dimension || pair.second.size() != dimension) {
      throw std::invalid_argument(
          "linear matroid parity vectors must share one dimension");
    }
    for (std::uint64_t& value : pair.first) {
      value %= modulus;
    }
    for (std::uint64_t& value : pair.second) {
      value %= modulus;
    }
  }

  linear_matroid_parity_detail::Solver solver(std::move(pairs), modulus,
                                               dimension);
  return solver.solve();
}

}  // namespace algorithms::combinatorial
