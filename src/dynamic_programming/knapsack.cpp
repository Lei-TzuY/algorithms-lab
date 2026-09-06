#include "algorithms/dynamic_programming/knapsack.hpp"

#include <limits>
#include <stdexcept>
#include <vector>

namespace algorithms::dynamic_programming {
namespace {

std::int64_t checked_add(std::int64_t left, std::int64_t right) {
  if ((right > 0 && left > std::numeric_limits<std::int64_t>::max() - right) ||
      (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right)) {
    throw std::overflow_error("knapsack value overflow");
  }
  return left + right;
}

std::size_t checked_columns(std::size_t capacity) {
  if (capacity == std::numeric_limits<std::size_t>::max()) {
    throw std::length_error("knapsack capacity is too large");
  }
  return capacity + 1;
}

std::size_t checked_table_size(std::size_t item_count, std::size_t capacity) {
  if (item_count == std::numeric_limits<std::size_t>::max()) {
    throw std::length_error("knapsack item count is too large");
  }
  const std::size_t rows = item_count + 1;
  const std::size_t columns = checked_columns(capacity);
  if (rows > std::numeric_limits<std::size_t>::max() / columns) {
    throw std::length_error("knapsack DP table dimensions overflow");
  }
  return rows * columns;
}

class Table {
 public:
  Table(std::size_t item_count, std::size_t capacity)
      : columns_(checked_columns(capacity)),
        values_(checked_table_size(item_count, capacity), 0) {}

  std::int64_t& at(std::size_t row, std::size_t capacity) {
    return values_[row * columns_ + capacity];
  }

  [[nodiscard]] std::int64_t at(std::size_t row,
                                std::size_t capacity) const {
    return values_[row * columns_ + capacity];
  }

 private:
  std::size_t columns_;
  std::vector<std::int64_t> values_;
};

}  // namespace

ZeroOneKnapsackResult solve_zero_one_knapsack(
    const std::vector<KnapsackItem>& items, std::size_t capacity) {
  Table dp(items.size(), capacity);

  for (std::size_t item_count = 1; item_count <= items.size(); ++item_count) {
    const KnapsackItem& item = items[item_count - 1];
    for (std::size_t current_capacity = 0; current_capacity <= capacity;
         ++current_capacity) {
      const std::int64_t excluded = dp.at(item_count - 1, current_capacity);
      std::int64_t best = excluded;
      if (item.weight <= current_capacity) {
        const std::int64_t included = checked_add(
            dp.at(item_count - 1, current_capacity - item.weight), item.value);
        if (included > best) {
          best = included;
        }
      }
      dp.at(item_count, current_capacity) = best;
    }
  }

  ZeroOneKnapsackResult result;
  result.best_value = dp.at(items.size(), capacity);

  std::size_t remaining_capacity = capacity;
  for (std::size_t item_count = items.size(); item_count > 0; --item_count) {
    if (dp.at(item_count, remaining_capacity) ==
        dp.at(item_count - 1, remaining_capacity)) {
      continue;
    }

    const std::size_t index = item_count - 1;
    const KnapsackItem& item = items[index];
    result.selected_indices.push_back(index);
    result.total_weight += item.weight;
    remaining_capacity -= item.weight;
  }

  // Reconstruction walks rows backwards; expose indices in input order.
  for (std::size_t left = 0, right = result.selected_indices.size(); left < right;
       ++left) {
    --right;
    if (left >= right) {
      break;
    }
    const std::size_t temporary = result.selected_indices[left];
    result.selected_indices[left] = result.selected_indices[right];
    result.selected_indices[right] = temporary;
  }
  return result;
}

UnboundedKnapsackResult solve_unbounded_knapsack(
    const std::vector<KnapsackItem>& items, std::size_t capacity) {
  for (const KnapsackItem& item : items) {
    if (item.weight == 0) {
      throw std::invalid_argument(
          "unbounded knapsack requires strictly positive item weights");
    }
  }

  Table dp(items.size(), capacity);

  for (std::size_t item_count = 1; item_count <= items.size(); ++item_count) {
    const KnapsackItem& item = items[item_count - 1];
    for (std::size_t current_capacity = 0; current_capacity <= capacity;
         ++current_capacity) {
      const std::int64_t excluded = dp.at(item_count - 1, current_capacity);
      std::int64_t best = excluded;
      if (item.weight <= current_capacity) {
        const std::int64_t included = checked_add(
            dp.at(item_count, current_capacity - item.weight), item.value);
        if (included > best) {
          best = included;
        }
      }
      dp.at(item_count, current_capacity) = best;
    }
  }

  UnboundedKnapsackResult result;
  result.best_value = dp.at(items.size(), capacity);
  result.item_counts.assign(items.size(), 0);

  std::size_t item_count = items.size();
  std::size_t remaining_capacity = capacity;
  while (item_count > 0) {
    if (dp.at(item_count, remaining_capacity) ==
        dp.at(item_count - 1, remaining_capacity)) {
      --item_count;
      continue;
    }

    const std::size_t index = item_count - 1;
    const KnapsackItem& item = items[index];
    ++result.item_counts[index];
    result.total_weight += item.weight;
    remaining_capacity -= item.weight;
  }
  return result;
}

}  // namespace algorithms::dynamic_programming
