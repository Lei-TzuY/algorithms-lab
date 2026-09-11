#include "algorithms/combinatorial/stable_matching.hpp"

#include <algorithm>
#include <deque>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace algorithms::combinatorial {
namespace {

void validate_preferences(
    const std::vector<std::vector<std::size_t>>& preferences,
    const std::size_t participant_count, const std::string_view side_name) {
  if (preferences.size() != participant_count) {
    throw std::invalid_argument(std::string(side_name) +
                                " preference row count must equal participant count");
  }

  std::vector<bool> seen(participant_count, false);
  for (const auto& row : preferences) {
    if (row.size() != participant_count) {
      throw std::invalid_argument(std::string(side_name) +
                                  " preference row must contain every participant exactly once");
    }

    std::fill(seen.begin(), seen.end(), false);
    for (const std::size_t candidate : row) {
      if (candidate >= participant_count) {
        throw std::out_of_range(std::string(side_name) +
                                " preference contains out-of-range participant");
      }
      if (seen[candidate]) {
        throw std::invalid_argument(std::string(side_name) +
                                    " preference row contains a duplicate participant");
      }
      seen[candidate] = true;
    }
  }
}

}  // namespace

StableMatchingResult gale_shapley_stable_matching(
    const std::vector<std::vector<std::size_t>>& proposer_preferences,
    const std::vector<std::vector<std::size_t>>& receiver_preferences) {
  const std::size_t participant_count = proposer_preferences.size();
  if (receiver_preferences.size() != participant_count) {
    throw std::invalid_argument("stable matching requires equally sized sides");
  }

  validate_preferences(proposer_preferences, participant_count, "proposer");
  validate_preferences(receiver_preferences, participant_count, "receiver");

  StableMatchingResult result;
  result.proposer_to_receiver.assign(participant_count, participant_count);
  result.receiver_to_proposer.assign(participant_count, participant_count);
  if (participant_count == 0U) {
    return result;
  }

  std::vector<std::vector<std::size_t>> receiver_rank(
      participant_count, std::vector<std::size_t>(participant_count));
  for (std::size_t receiver = 0; receiver < participant_count; ++receiver) {
    for (std::size_t rank = 0; rank < participant_count; ++rank) {
      receiver_rank[receiver][receiver_preferences[receiver][rank]] = rank;
    }
  }

  std::vector<std::size_t> next_choice(participant_count, 0U);
  std::deque<std::size_t> free_proposers;
  for (std::size_t proposer = 0; proposer < participant_count; ++proposer) {
    free_proposers.push_back(proposer);
  }

  while (!free_proposers.empty()) {
    const std::size_t proposer = free_proposers.front();
    free_proposers.pop_front();

    if (next_choice[proposer] >= participant_count) {
      throw std::logic_error(
          "complete preference lists exhausted before a perfect matching formed");
    }

    const std::size_t receiver =
        proposer_preferences[proposer][next_choice[proposer]];
    ++next_choice[proposer];
    if (result.proposal_count == std::numeric_limits<std::size_t>::max()) {
      throw std::overflow_error("stable-matching proposal counter overflow");
    }
    ++result.proposal_count;

    const std::size_t incumbent = result.receiver_to_proposer[receiver];
    if (incumbent == participant_count) {
      result.receiver_to_proposer[receiver] = proposer;
      result.proposer_to_receiver[proposer] = receiver;
      continue;
    }

    if (receiver_rank[receiver][proposer] <
        receiver_rank[receiver][incumbent]) {
      result.proposer_to_receiver[incumbent] = participant_count;
      free_proposers.push_back(incumbent);
      result.receiver_to_proposer[receiver] = proposer;
      result.proposer_to_receiver[proposer] = receiver;
    } else {
      free_proposers.push_back(proposer);
    }
  }

  for (std::size_t proposer = 0; proposer < participant_count; ++proposer) {
    if (result.proposer_to_receiver[proposer] == participant_count) {
      throw std::logic_error("stable matching failed to match every proposer");
    }
  }

  return result;
}

}  // namespace algorithms::combinatorial
