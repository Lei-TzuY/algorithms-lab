#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace algorithms::dynamic_programming {

enum class EditKind {
  match,
  substitute,
  erase,
  insert,
};

struct EditOperation {
  EditKind kind;
  char source_character;
  char target_character;

  friend bool operator==(const EditOperation&, const EditOperation&) = default;
};

struct EditDistanceResult {
  std::size_t distance{0};
  std::vector<EditOperation> operations;
};

// Levenshtein edit distance over byte sequences represented by string_view.
// Match costs 0; substitution, erase, and insertion each cost 1.
//
// State invariant: dp[i][j] is the minimum edit cost transforming source[0,i)
// into target[0,j). Every optimal final operation is one of match/substitute,
// erase, or insert, so the recurrence considers all possible final steps.
// Reconstruction is deterministic: match is forced when characters are equal;
// otherwise ties prefer substitute, then erase, then insert.
//
// Time: O(|source| * |target|), space: O(|source| * |target|).
[[nodiscard]] EditDistanceResult levenshtein_edit_distance(
    std::string_view source, std::string_view target);

}  // namespace algorithms::dynamic_programming
