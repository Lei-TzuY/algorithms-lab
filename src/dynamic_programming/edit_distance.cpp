#include "algorithms/dynamic_programming/edit_distance.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>

namespace algorithms::dynamic_programming {
namespace {

std::size_t checked_dimension(std::size_t size) {
  if (size == std::numeric_limits<std::size_t>::max()) {
    throw std::length_error("edit-distance dimension is too large");
  }
  return size + 1;
}

std::size_t checked_table_size(std::size_t source_size,
                               std::size_t target_size) {
  const std::size_t rows = checked_dimension(source_size);
  const std::size_t columns = checked_dimension(target_size);
  if (rows > std::numeric_limits<std::size_t>::max() / columns) {
    throw std::length_error("edit-distance table dimensions overflow");
  }
  return rows * columns;
}

class Table {
 public:
  Table(std::size_t source_size, std::size_t target_size)
      : columns_(checked_dimension(target_size)),
        values_(checked_table_size(source_size, target_size), 0) {}

  std::size_t& at(std::size_t source_prefix, std::size_t target_prefix) {
    return values_[source_prefix * columns_ + target_prefix];
  }

  [[nodiscard]] std::size_t at(std::size_t source_prefix,
                               std::size_t target_prefix) const {
    return values_[source_prefix * columns_ + target_prefix];
  }

 private:
  std::size_t columns_;
  std::vector<std::size_t> values_;
};

}  // namespace

EditDistanceResult levenshtein_edit_distance(std::string_view source,
                                             std::string_view target) {
  Table dp(source.size(), target.size());
  for (std::size_t source_prefix = 0; source_prefix <= source.size();
       ++source_prefix) {
    dp.at(source_prefix, 0) = source_prefix;
  }
  for (std::size_t target_prefix = 0; target_prefix <= target.size();
       ++target_prefix) {
    dp.at(0, target_prefix) = target_prefix;
  }

  for (std::size_t source_prefix = 1; source_prefix <= source.size();
       ++source_prefix) {
    for (std::size_t target_prefix = 1; target_prefix <= target.size();
         ++target_prefix) {
      if (source[source_prefix - 1] == target[target_prefix - 1]) {
        dp.at(source_prefix, target_prefix) =
            dp.at(source_prefix - 1, target_prefix - 1);
        continue;
      }

      const std::size_t substitute =
          dp.at(source_prefix - 1, target_prefix - 1) + 1;
      const std::size_t erase = dp.at(source_prefix - 1, target_prefix) + 1;
      const std::size_t insert = dp.at(source_prefix, target_prefix - 1) + 1;
      dp.at(source_prefix, target_prefix) =
          std::min(substitute, std::min(erase, insert));
    }
  }

  EditDistanceResult result;
  result.distance = dp.at(source.size(), target.size());

  std::size_t source_prefix = source.size();
  std::size_t target_prefix = target.size();
  while (source_prefix > 0 || target_prefix > 0) {
    if (source_prefix > 0 && target_prefix > 0 &&
        source[source_prefix - 1] == target[target_prefix - 1] &&
        dp.at(source_prefix, target_prefix) ==
            dp.at(source_prefix - 1, target_prefix - 1)) {
      result.operations.push_back(EditOperation{
          EditKind::match, source[source_prefix - 1], target[target_prefix - 1]});
      --source_prefix;
      --target_prefix;
      continue;
    }

    const std::size_t current = dp.at(source_prefix, target_prefix);
    if (source_prefix > 0 && target_prefix > 0 &&
        dp.at(source_prefix - 1, target_prefix - 1) + 1 == current) {
      result.operations.push_back(
          EditOperation{EditKind::substitute, source[source_prefix - 1],
                        target[target_prefix - 1]});
      --source_prefix;
      --target_prefix;
    } else if (source_prefix > 0 &&
               dp.at(source_prefix - 1, target_prefix) + 1 == current) {
      result.operations.push_back(EditOperation{
          EditKind::erase, source[source_prefix - 1], '\0'});
      --source_prefix;
    } else if (target_prefix > 0 &&
               dp.at(source_prefix, target_prefix - 1) + 1 == current) {
      result.operations.push_back(EditOperation{
          EditKind::insert, '\0', target[target_prefix - 1]});
      --target_prefix;
    } else {
      throw std::logic_error("edit-distance reconstruction invariant violated");
    }
  }

  std::reverse(result.operations.begin(), result.operations.end());
  return result;
}

}  // namespace algorithms::dynamic_programming
