#include "algorithms/offline/mo_distinct.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace algorithms::offline {
namespace {

struct IndexedQuery {
  RangeQuery range;
  std::size_t index;
};

std::size_t block_size_for(std::size_t value_count) {
  if (value_count <= 1U) {
    return 1U;
  }

  std::size_t low = 1U;
  std::size_t high = value_count;
  std::size_t answer = 1U;
  while (low <= high) {
    const std::size_t middle = low + (high - low) / 2U;
    if (middle <= value_count / middle) {
      answer = middle;
      low = middle + 1U;
    } else {
      high = middle - 1U;
    }
  }
  return answer;
}

}  // namespace

std::vector<std::size_t> mo_distinct_counts(
    std::span<const std::int64_t> values,
    std::span<const RangeQuery> queries) {
  const std::size_t value_count = values.size();
  std::vector<IndexedQuery> ordered;
  ordered.reserve(queries.size());

  for (std::size_t index = 0; index < queries.size(); ++index) {
    const RangeQuery range = queries[index];
    if (range.begin > range.end) {
      throw std::invalid_argument("range begin exceeds end");
    }
    if (range.end > value_count) {
      throw std::out_of_range("range endpoint exceeds value count");
    }
    ordered.push_back(IndexedQuery{range, index});
  }

  if (queries.empty()) {
    return {};
  }

  std::vector<std::int64_t> dictionary(values.begin(), values.end());
  std::sort(dictionary.begin(), dictionary.end());
  dictionary.erase(std::unique(dictionary.begin(), dictionary.end()),
                   dictionary.end());

  std::vector<std::size_t> compressed;
  compressed.reserve(value_count);
  for (const std::int64_t value : values) {
    const auto iterator = std::lower_bound(dictionary.begin(), dictionary.end(), value);
    compressed.push_back(static_cast<std::size_t>(iterator - dictionary.begin()));
  }

  const std::size_t block_size = block_size_for(value_count);
  std::sort(ordered.begin(), ordered.end(),
            [block_size](const IndexedQuery& left, const IndexedQuery& right) {
              const std::size_t left_block = left.range.begin / block_size;
              const std::size_t right_block = right.range.begin / block_size;
              if (left_block != right_block) {
                return left_block < right_block;
              }
              if (left.range.end != right.range.end) {
                if ((left_block & 1U) == 0U) {
                  return left.range.end < right.range.end;
                }
                return left.range.end > right.range.end;
              }
              return left.index < right.index;
            });

  std::vector<std::size_t> frequency(dictionary.size(), 0U);
  std::vector<std::size_t> answers(queries.size(), 0U);
  std::size_t distinct = 0U;
  std::size_t current_begin = 0U;
  std::size_t current_end = 0U;

  const auto add = [&](std::size_t position) {
    const std::size_t symbol = compressed[position];
    if (frequency[symbol] == 0U) {
      ++distinct;
    }
    ++frequency[symbol];
  };

  const auto remove = [&](std::size_t position) {
    const std::size_t symbol = compressed[position];
    if (frequency[symbol] == 0U) {
      throw std::logic_error("Mo frequency invariant violated");
    }
    --frequency[symbol];
    if (frequency[symbol] == 0U) {
      --distinct;
    }
  };

  for (const IndexedQuery& query : ordered) {
    while (current_begin > query.range.begin) {
      --current_begin;
      add(current_begin);
    }
    while (current_end < query.range.end) {
      add(current_end);
      ++current_end;
    }
    while (current_begin < query.range.begin) {
      remove(current_begin);
      ++current_begin;
    }
    while (current_end > query.range.end) {
      --current_end;
      remove(current_end);
    }
    answers[query.index] = distinct;
  }

  return answers;
}

}  // namespace algorithms::offline
