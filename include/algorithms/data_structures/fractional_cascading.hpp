#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

class FractionalCascadingIndex {
 public:
  using Value = std::int64_t;

  explicit FractionalCascadingIndex(std::vector<std::vector<Value>> catalogs)
      : catalogs_(std::move(catalogs)), levels_(catalogs_.size()) {
    for (const auto& catalog : catalogs_) {
      for (std::size_t i = 1; i < catalog.size(); ++i) {
        if (catalog[i] < catalog[i - 1]) {
          throw std::invalid_argument(
              "fractional cascading catalogs must be nondecreasing");
        }
      }
    }
    if (catalogs_.empty()) {
      return;
    }

    const std::size_t last = catalogs_.size() - 1U;
    levels_[last].reserve(catalogs_[last].size());
    for (const Value value : catalogs_[last]) {
      levels_[last].push_back(Entry{value, 0U, 0U});
    }
    populate_bridges(last);

    for (std::size_t level = last; level-- > 0U;) {
      build_level(level);
      populate_bridges(level);
    }
  }

  [[nodiscard]] std::size_t catalog_count() const noexcept {
    return catalogs_.size();
  }

  [[nodiscard]] const std::vector<std::vector<Value>>& catalogs() const noexcept {
    return catalogs_;
  }

  [[nodiscard]] std::vector<std::size_t> augmented_sizes() const {
    std::vector<std::size_t> result;
    result.reserve(levels_.size());
    for (const auto& level : levels_) {
      result.push_back(level.size());
    }
    return result;
  }

  [[nodiscard]] std::size_t total_augmented_entries() const noexcept {
    std::size_t total = 0U;
    for (const auto& level : levels_) {
      total += level.size();
    }
    return total;
  }

  // Returns the lower_bound insertion index for the same key in every catalog.
  // One binary search is performed at the first augmented level; every later
  // catalog is reached through a stored bridge plus at most one predecessor
  // correction because every second next-level entry is cascaded upward.
  [[nodiscard]] std::vector<std::size_t> lower_bound_indices(Value key) const {
    std::vector<std::size_t> result(catalogs_.size(), 0U);
    if (catalogs_.empty()) {
      return result;
    }

    std::size_t position = lower_bound_level(levels_[0], key);
    for (std::size_t level = 0; level < levels_.size(); ++level) {
      const auto& augmented = levels_[level];
      result[level] = position == augmented.size()
                          ? catalogs_[level].size()
                          : augmented[position].original_lower_bound;

      if (level + 1U == levels_.size()) {
        break;
      }

      const auto& next = levels_[level + 1U];
      std::size_t next_position =
          position == augmented.size() ? next.size()
                                       : augmented[position].next_lower_bound;
      if (next_position > 0U && next[next_position - 1U].value >= key) {
        --next_position;
      }
      position = next_position;
    }
    return result;
  }

  // Expensive structural diagnostic. It is intentionally excluded from query
  // complexity claims.
  [[nodiscard]] bool valid_structure() const {
    if (levels_.size() != catalogs_.size()) {
      return false;
    }
    for (std::size_t level = 0; level < levels_.size(); ++level) {
      const auto& current = levels_[level];
      for (std::size_t i = 1; i < current.size(); ++i) {
        if (current[i].value < current[i - 1U].value) {
          return false;
        }
      }

      const std::size_t sampled_next =
          level + 1U < levels_.size() ? levels_[level + 1U].size() / 2U : 0U;
      if (current.size() != catalogs_[level].size() + sampled_next) {
        return false;
      }

      std::size_t catalog_cursor = 0U;
      std::size_t next_cursor = 0U;
      std::size_t sampled_cursor = 1U;
      for (const Entry& entry : current) {
        while (catalog_cursor < catalogs_[level].size() &&
               catalogs_[level][catalog_cursor] < entry.value) {
          ++catalog_cursor;
        }
        if (entry.original_lower_bound != catalog_cursor) {
          return false;
        }

        if (level + 1U < levels_.size()) {
          const auto& next = levels_[level + 1U];
          while (next_cursor < next.size() && next[next_cursor].value < entry.value) {
            ++next_cursor;
          }
          if (entry.next_lower_bound != next_cursor) {
            return false;
          }
        } else if (entry.next_lower_bound != 0U) {
          return false;
        }
      }

      // Both source multisets must occur as ordered subsequences of the merged
      // augmented list. Combined with sortedness and exact size, this replays
      // the merge content without rebuilding it through the production helper.
      std::size_t merged_cursor = 0U;
      for (const Value value : catalogs_[level]) {
        while (merged_cursor < current.size() && current[merged_cursor].value != value) {
          ++merged_cursor;
        }
        if (merged_cursor == current.size()) {
          return false;
        }
        ++merged_cursor;
      }
      if (level + 1U < levels_.size()) {
        merged_cursor = 0U;
        const auto& next = levels_[level + 1U];
        for (sampled_cursor = 1U; sampled_cursor < next.size(); sampled_cursor += 2U) {
          const Value value = next[sampled_cursor].value;
          while (merged_cursor < current.size() && current[merged_cursor].value != value) {
            ++merged_cursor;
          }
          if (merged_cursor == current.size()) {
            return false;
          }
          ++merged_cursor;
        }
      }
    }
    return true;
  }

 private:
  struct Entry {
    Value value{};
    std::size_t original_lower_bound{};
    std::size_t next_lower_bound{};
  };

  static std::size_t lower_bound_level(const std::vector<Entry>& level,
                                       Value key) noexcept {
    std::size_t low = 0U;
    std::size_t high = level.size();
    while (low < high) {
      const std::size_t mid = low + (high - low) / 2U;
      if (level[mid].value < key) {
        low = mid + 1U;
      } else {
        high = mid;
      }
    }
    return low;
  }

  void build_level(std::size_t level) {
    const auto& catalog = catalogs_[level];
    const auto& next = levels_[level + 1U];
    auto& current = levels_[level];
    current.clear();
    current.reserve(catalog.size() + next.size() / 2U);

    std::size_t catalog_index = 0U;
    std::size_t sampled_index = 1U;
    while (catalog_index < catalog.size() || sampled_index < next.size()) {
      const bool take_catalog =
          sampled_index >= next.size() ||
          (catalog_index < catalog.size() &&
           catalog[catalog_index] <= next[sampled_index].value);
      if (take_catalog) {
        current.push_back(Entry{catalog[catalog_index], 0U, 0U});
        ++catalog_index;
      } else {
        current.push_back(Entry{next[sampled_index].value, 0U, 0U});
        sampled_index += 2U;
      }
    }
  }

  void populate_bridges(std::size_t level) {
    auto& current = levels_[level];
    const auto& catalog = catalogs_[level];
    std::size_t original = 0U;
    std::size_t next = 0U;
    for (Entry& entry : current) {
      while (original < catalog.size() && catalog[original] < entry.value) {
        ++original;
      }
      entry.original_lower_bound = original;

      if (level + 1U < levels_.size()) {
        const auto& next_level = levels_[level + 1U];
        while (next < next_level.size() && next_level[next].value < entry.value) {
          ++next;
        }
        entry.next_lower_bound = next;
      } else {
        entry.next_lower_bound = 0U;
      }
    }
  }

  std::vector<std::vector<Value>> catalogs_;
  std::vector<std::vector<Entry>> levels_;
};

}  // namespace algorithms::data_structures
