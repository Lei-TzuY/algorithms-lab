#pragma once

#include <cstddef>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::combinatorial {

using StandardTableau = std::vector<std::vector<std::size_t>>;

struct RobinsonSchenstedResult {
  StandardTableau insertion_tableau;
  StandardTableau recording_tableau;
};

namespace detail {

inline bool checked_cell_count(const StandardTableau& tableau,
                               std::size_t& count) noexcept {
  count = 0;
  for (const auto& row : tableau) {
    if (row.size() > std::numeric_limits<std::size_t>::max() - count) {
      return false;
    }
    count += row.size();
  }
  return true;
}

inline bool is_standard_tableau(const StandardTableau& tableau,
                                std::size_t expected_cells) {
  std::size_t cell_count = 0;
  if (!checked_cell_count(tableau, cell_count) || cell_count != expected_cells) {
    return false;
  }

  if (!tableau.empty() && tableau.front().empty()) {
    return false;
  }
  for (std::size_t row = 1; row < tableau.size(); ++row) {
    if (tableau[row].empty() || tableau[row].size() > tableau[row - 1].size()) {
      return false;
    }
  }

  std::vector<bool> seen(expected_cells, false);
  for (std::size_t row = 0; row < tableau.size(); ++row) {
    for (std::size_t column = 0; column < tableau[row].size(); ++column) {
      const std::size_t value = tableau[row][column];
      if (value >= expected_cells || seen[value]) {
        return false;
      }
      seen[value] = true;
      if (column != 0 && tableau[row][column - 1] >= value) {
        return false;
      }
      if (row != 0 && column < tableau[row - 1].size() &&
          tableau[row - 1][column] >= value) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace detail

[[nodiscard]] inline bool valid_robinson_schensted_result(
    const RobinsonSchenstedResult& result) {
  std::size_t insertion_cells = 0;
  std::size_t recording_cells = 0;
  if (!detail::checked_cell_count(result.insertion_tableau, insertion_cells) ||
      !detail::checked_cell_count(result.recording_tableau, recording_cells) ||
      insertion_cells != recording_cells ||
      result.insertion_tableau.size() != result.recording_tableau.size()) {
    return false;
  }

  for (std::size_t row = 0; row < result.insertion_tableau.size(); ++row) {
    if (result.insertion_tableau[row].size() !=
        result.recording_tableau[row].size()) {
      return false;
    }
  }

  return detail::is_standard_tableau(result.insertion_tableau,
                                     insertion_cells) &&
         detail::is_standard_tableau(result.recording_tableau,
                                     recording_cells);
}

[[nodiscard]] inline RobinsonSchenstedResult robinson_schensted_permutation(
    std::span<const std::size_t> permutation) {
  const std::size_t n = permutation.size();
  std::vector<bool> seen(n, false);
  for (const std::size_t value : permutation) {
    if (value >= n || seen[value]) {
      throw std::invalid_argument(
          "Robinson-Schensted input must be a permutation of [0,n)");
    }
    seen[value] = true;
  }

  RobinsonSchenstedResult result;
  for (std::size_t insertion_index = 0; insertion_index < n;
       ++insertion_index) {
    std::size_t carried = permutation[insertion_index];
    std::size_t row_index = 0;

    while (true) {
      if (row_index == result.insertion_tableau.size()) {
        result.insertion_tableau.push_back({carried});
        result.recording_tableau.push_back({insertion_index});
        break;
      }

      auto& row = result.insertion_tableau[row_index];
      std::size_t column = 0;
      while (column < row.size() && row[column] < carried) {
        ++column;
      }

      if (column == row.size()) {
        row.push_back(carried);
        result.recording_tableau[row_index].push_back(insertion_index);
        break;
      }

      std::swap(carried, row[column]);
      ++row_index;
    }
  }

  return result;
}

[[nodiscard]] inline std::vector<std::size_t>
 inverse_robinson_schensted_permutation(const RobinsonSchenstedResult& result) {
  if (!valid_robinson_schensted_result(result)) {
    throw std::invalid_argument(
        "inverse Robinson-Schensted requires standard tableaux of equal shape");
  }

  StandardTableau insertion = result.insertion_tableau;
  StandardTableau recording = result.recording_tableau;
  std::size_t n = 0;
  static_cast<void>(detail::checked_cell_count(insertion, n));
  std::vector<std::size_t> permutation(n, 0);

  for (std::size_t remaining = n; remaining != 0; --remaining) {
    const std::size_t label = remaining - 1;
    std::size_t row_index = recording.size();
    for (std::size_t row = 0; row < recording.size(); ++row) {
      if (!recording[row].empty() && recording[row].back() == label) {
        row_index = row;
        break;
      }
    }
    if (row_index == recording.size()) {
      throw std::invalid_argument("recording tableau has no removable corner");
    }

    std::size_t carried = insertion[row_index].back();
    insertion[row_index].pop_back();
    recording[row_index].pop_back();

    for (std::size_t row = row_index; row != 0; --row) {
      auto& upper = insertion[row - 1];
      std::size_t column = upper.size();
      while (column != 0 && upper[column - 1] >= carried) {
        --column;
      }
      if (column == 0) {
        throw std::invalid_argument(
            "tableaux do not admit Robinson-Schensted reverse insertion");
      }
      --column;
      std::swap(carried, upper[column]);
    }

    permutation[label] = carried;

    while (!insertion.empty() && insertion.back().empty()) {
      insertion.pop_back();
      recording.pop_back();
    }
  }

  return permutation;
}

}  // namespace algorithms::combinatorial
