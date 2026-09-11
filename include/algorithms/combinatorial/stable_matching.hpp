#pragma once

#include <cstddef>
#include <vector>

namespace algorithms::combinatorial {

struct StableMatchingResult {
  std::vector<std::size_t> proposer_to_receiver;
  std::vector<std::size_t> receiver_to_proposer;
  std::size_t proposal_count{};

  friend bool operator==(const StableMatchingResult&, const StableMatchingResult&) =
      default;
};

// Gale-Shapley with the proposer side making proposals.
//
// Preconditions enforced at runtime:
// - both sides contain the same number n of participants;
// - every preference row is a strict permutation of [0, n).
//
// The returned perfect matching is stable and proposer-optimal among all stable
// matchings for the supplied strict complete preferences.
[[nodiscard]] StableMatchingResult gale_shapley_stable_matching(
    const std::vector<std::vector<std::size_t>>& proposer_preferences,
    const std::vector<std::vector<std::size_t>>& receiver_preferences);

}  // namespace algorithms::combinatorial
