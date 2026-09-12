#pragma once

#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace algorithms::combinatorial {

struct StableRoommatesResult {
  std::vector<std::size_t> partner;
  std::size_t phase1_proposal_count{};
  std::size_t rotation_count{};

  friend bool operator==(const StableRoommatesResult&,
                         const StableRoommatesResult&) = default;
};

[[nodiscard]] inline std::optional<StableRoommatesResult> stable_roommates_irving(
    const std::vector<std::vector<std::size_t>>& preferences) {
  const std::size_t n = preferences.size();
  for (std::size_t person = 0; person < n; ++person) {
    const auto& row = preferences[person];
    if (row.size() != n - 1U) {
      throw std::invalid_argument(
          "stable roommates preference row must list every other participant exactly once");
    }
    std::vector<bool> seen(n, false);
    for (const std::size_t candidate : row) {
      if (candidate >= n) {
        throw std::out_of_range(
            "stable roommates preference contains out-of-range participant");
      }
      if (candidate == person) {
        throw std::invalid_argument(
            "stable roommates preference row must not contain self");
      }
      if (seen[candidate]) {
        throw std::invalid_argument(
            "stable roommates preference row contains duplicate participant");
      }
      seen[candidate] = true;
    }
  }

  if (n == 0U) {
    return StableRoommatesResult{};
  }
  if ((n & 1U) != 0U) {
    return std::nullopt;
  }

  std::vector<std::vector<std::size_t>> rank(
      n, std::vector<std::size_t>(n, n));
  for (std::size_t person = 0; person < n; ++person) {
    for (std::size_t position = 0; position < preferences[person].size();
         ++position) {
      rank[person][preferences[person][position]] = position;
    }
  }

  // Phase 1: directed semi-engagements plus immediate successor deletion.
  // Once recipient y holds proposer x, every participant ranked below x by y is
  // removed symmetrically. If such a deletion breaks y's own outgoing
  // semi-engagement, y becomes free and proposes again.
  constexpr std::size_t kNone = std::numeric_limits<std::size_t>::max();
  std::vector<std::vector<bool>> active(n, std::vector<bool>(n, false));
  for (std::size_t person = 0; person < n; ++person) {
    for (const std::size_t candidate : preferences[person]) {
      active[person][candidate] = true;
    }
  }

  std::vector<std::size_t> held(n, kNone);
  std::vector<std::size_t> proposal_to(n, kNone);
  std::vector<std::size_t> queue;
  queue.reserve(n);
  for (std::size_t person = 0; person < n; ++person) queue.push_back(person);
  std::size_t queue_head = 0U;
  std::size_t proposal_count = 0U;

  const auto enqueue_if_freed = [&](const std::size_t proposer) {
    if (proposal_to[proposer] == kNone) queue.push_back(proposer);
  };

  const auto erase_phase1_pair = [&](const std::size_t a,
                                      const std::size_t b) {
    if (!active[a][b]) return;
    active[a][b] = false;
    active[b][a] = false;
    if (proposal_to[a] == b) {
      proposal_to[a] = kNone;
      if (held[b] == a) held[b] = kNone;
      enqueue_if_freed(a);
    }
    if (proposal_to[b] == a) {
      proposal_to[b] = kNone;
      if (held[a] == b) held[a] = kNone;
      enqueue_if_freed(b);
    }
  };

  const auto current_first = [&](const std::size_t person) {
    for (const std::size_t candidate : preferences[person]) {
      if (active[person][candidate]) return candidate;
    }
    return kNone;
  };

  while (queue_head < queue.size()) {
    const std::size_t proposer = queue[queue_head++];
    if (proposal_to[proposer] != kNone) continue;
    const std::size_t recipient = current_first(proposer);
    if (recipient == kNone) return std::nullopt;

    if (proposal_count == std::numeric_limits<std::size_t>::max()) {
      throw std::overflow_error("stable roommates proposal counter overflow");
    }
    ++proposal_count;

    const std::size_t incumbent = held[recipient];
    if (incumbent != kNone) {
      // Because recipient deleted every successor of incumbent when that
      // proposal was accepted, any still-active new proposer is preferred to
      // the incumbent.
      if (rank[recipient][proposer] >= rank[recipient][incumbent]) {
        throw std::logic_error(
            "stable roommates Phase-1 table retained a rejected successor");
      }
      erase_phase1_pair(incumbent, recipient);
    }
    held[recipient] = proposer;
    proposal_to[proposer] = recipient;

    const std::size_t cutoff = rank[recipient][proposer];
    for (std::size_t pos = cutoff + 1U; pos < n - 1U; ++pos) {
      erase_phase1_pair(recipient, preferences[recipient][pos]);
    }
  }

  const auto erase_pair = [&](const std::size_t a, const std::size_t b) {
    active[a][b] = false;
    active[b][a] = false;
  };

  const auto list_size = [&](const std::size_t person) {
    std::size_t count = 0U;
    for (const std::size_t candidate : preferences[person]) {
      if (active[person][candidate]) ++count;
    }
    return count;
  };
  const auto first_choice = [&](const std::size_t person) {
    for (const std::size_t candidate : preferences[person]) {
      if (active[person][candidate]) return candidate;
    }
    return kNone;
  };
  const auto second_choice = [&](const std::size_t person) {
    bool saw_first = false;
    for (const std::size_t candidate : preferences[person]) {
      if (!active[person][candidate]) continue;
      if (!saw_first) {
        saw_first = true;
      } else {
        return candidate;
      }
    }
    return kNone;
  };
  const auto last_choice = [&](const std::size_t person) {
    for (auto it = preferences[person].rbegin();
         it != preferences[person].rend(); ++it) {
      if (active[person][*it]) return *it;
    }
    return kNone;
  };

  for (std::size_t person = 0; person < n; ++person) {
    if (list_size(person) == 0U) return std::nullopt;
  }

  std::size_t rotation_count = 0U;
  for (;;) {
    std::size_t start = kNone;
    for (std::size_t person = 0; person < n; ++person) {
      if (list_size(person) > 1U) {
        start = person;
        break;
      }
    }
    if (start == kNone) break;

    std::vector<std::size_t> seen(n, kNone);
    std::vector<std::size_t> xs;
    std::vector<std::size_t> ys;
    std::size_t current = start;
    std::size_t cycle_begin = kNone;
    for (;;) {
      if (seen[current] != kNone) {
        cycle_begin = seen[current];
        break;
      }
      const std::size_t second = second_choice(current);
      if (second == kNone) return std::nullopt;
      seen[current] = xs.size();
      xs.push_back(current);
      ys.push_back(second);
      current = last_choice(second);
      if (current == kNone) return std::nullopt;
    }

    if (cycle_begin >= xs.size()) {
      throw std::logic_error("stable roommates rotation discovery failed");
    }
    const std::size_t cycle_length = xs.size() - cycle_begin;
    std::vector<std::size_t> rotation_x(cycle_length);
    std::vector<std::size_t> rotation_new_y(cycle_length);
    std::vector<std::size_t> rotation_old_y(cycle_length);
    for (std::size_t offset = 0; offset < cycle_length; ++offset) {
      const std::size_t index = cycle_begin + offset;
      rotation_x[offset] = xs[index];
      rotation_new_y[offset] = ys[index];
      rotation_old_y[offset] = first_choice(xs[index]);
      if (rotation_old_y[offset] == kNone ||
          rotation_new_y[offset] == kNone) {
        return std::nullopt;
      }
    }

    // Eliminate the exposed rotation: every x_i loses its old first choice and
    // moves to its previous second choice y_{i+1}. Restoring the stable-table
    // invariant then deletes every successor of x_i from y_{i+1}'s list.
    for (std::size_t offset = 0; offset < cycle_length; ++offset) {
      erase_pair(rotation_x[offset], rotation_old_y[offset]);
    }
    for (std::size_t offset = 0; offset < cycle_length; ++offset) {
      const std::size_t x = rotation_x[offset];
      const std::size_t y = rotation_new_y[offset];
      if (!active[x][y]) return std::nullopt;
      const std::size_t cutoff = rank[y][x];
      for (std::size_t pos = cutoff + 1U; pos < n - 1U; ++pos) {
        const std::size_t successor = preferences[y][pos];
        if (active[y][successor]) erase_pair(y, successor);
      }
    }
    if (rotation_count == std::numeric_limits<std::size_t>::max()) {
      throw std::overflow_error("stable roommates rotation counter overflow");
    }
    ++rotation_count;

    for (std::size_t person = 0; person < n; ++person) {
      if (list_size(person) == 0U) return std::nullopt;
    }
  }

  StableRoommatesResult result;
  result.partner.resize(n, kNone);
  result.phase1_proposal_count = proposal_count;
  result.rotation_count = rotation_count;
  for (std::size_t person = 0; person < n; ++person) {
    const std::size_t partner = first_choice(person);
    if (partner == kNone || first_choice(partner) != person) {
      throw std::logic_error("stable roommates final table is not symmetric");
    }
    result.partner[person] = partner;
  }

  // Replay the public witness against the original preferences. This is a
  // fail-closed diagnostic, not a substitute for Irving's theorem.
  for (std::size_t person = 0; person < n; ++person) {
    const std::size_t mate = result.partner[person];
    for (const std::size_t candidate : preferences[person]) {
      if (candidate == mate) break;
      if (rank[candidate][person] < rank[candidate][result.partner[candidate]]) {
        throw std::logic_error("stable roommates produced a blocking pair");
      }
    }
  }
  return result;
}

}  // namespace algorithms::combinatorial
