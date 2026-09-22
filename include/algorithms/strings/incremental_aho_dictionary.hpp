#pragma once

#include "algorithms/strings/aho_corasick.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace algorithms::strings {

struct IncrementalAhoCorasickMatch {
  std::size_t pattern_id;
  std::size_t begin;
  std::size_t end;

  friend bool operator==(const IncrementalAhoCorasickMatch&,
                         const IncrementalAhoCorasickMatch&) = default;
};

// Insertion-only dynamic byte-pattern dictionary using the logarithmic method.
//
// Each occupied binary-counter bucket owns one immutable Aho-Corasick matcher.
// Inserting one pattern consolidates consecutive occupied buckets into the first
// empty level and rebuilds only that merged static automaton. Queries scan every
// occupied bucket and merge matches into one deterministic global-ID order.
class IncrementalAhoCorasickByteDictionary {
 public:
  IncrementalAhoCorasickByteDictionary() = default;
  IncrementalAhoCorasickByteDictionary(
      const IncrementalAhoCorasickByteDictionary&) = delete;
  IncrementalAhoCorasickByteDictionary& operator=(
      const IncrementalAhoCorasickByteDictionary&) = delete;
  IncrementalAhoCorasickByteDictionary(
      IncrementalAhoCorasickByteDictionary&&) noexcept = default;
  IncrementalAhoCorasickByteDictionary& operator=(
      IncrementalAhoCorasickByteDictionary&&) noexcept = default;

  [[nodiscard]] std::size_t pattern_count() const noexcept {
    return next_pattern_id_;
  }

  [[nodiscard]] std::size_t active_bucket_count() const noexcept {
    std::size_t count = 0U;
    for (const auto& bucket : buckets_) {
      if (bucket.has_value()) {
        ++count;
      }
    }
    return count;
  }

  [[nodiscard]] std::size_t add_pattern(std::string pattern) {
    if (next_pattern_id_ == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error("incremental Aho pattern id exhausted");
    }

    std::size_t level = 0U;
    while (level < buckets_.size() && buckets_[level].has_value()) {
      ++level;
    }

    std::size_t carry_size = 1U;
    for (std::size_t index = 0U; index < level; ++index) {
      const std::size_t existing = buckets_[index]->pattern_ids.size();
      if (existing > std::numeric_limits<std::size_t>::max() - carry_size) {
        throw std::length_error("incremental Aho bucket size exhausted");
      }
      carry_size += existing;
    }

    Bucket carry;
    carry.pattern_ids.reserve(carry_size);
    carry.patterns.reserve(carry_size);

    // Higher occupied levels contain older ID ranges. Iterating them in reverse
    // level order, then appending the new pattern, preserves global insertion
    // order inside the rebuilt static matcher.
    for (std::size_t cursor = level; cursor > 0U; --cursor) {
      const Bucket& existing = *buckets_[cursor - 1U];
      carry.pattern_ids.insert(carry.pattern_ids.end(),
                               existing.pattern_ids.begin(),
                               existing.pattern_ids.end());
      carry.patterns.insert(carry.patterns.end(), existing.patterns.begin(),
                            existing.patterns.end());
    }

    const std::size_t pattern_id = next_pattern_id_;
    carry.pattern_ids.push_back(pattern_id);
    carry.patterns.push_back(std::move(pattern));
    carry.matcher =
        std::make_unique<AhoCorasickByteMatcher>(carry.patterns);

    // All allocating/copying/rebuild work is complete before resident buckets
    // are changed. Vector resize provides the usual strong allocation boundary;
    // Bucket moves are composed only of vector/unique_ptr moves.
    if (level == buckets_.size()) {
      buckets_.resize(level + 1U);
    }
    for (std::size_t index = 0U; index < level; ++index) {
      buckets_[index].reset();
    }
    buckets_[level] = std::move(carry);
    ++next_pattern_id_;
    return pattern_id;
  }

  [[nodiscard]] std::vector<IncrementalAhoCorasickMatch> find_all(
      const std::string_view text) const {
    std::vector<IncrementalAhoCorasickMatch> matches;

    for (const auto& bucket : buckets_) {
      if (!bucket.has_value()) {
        continue;
      }
      const auto local_matches = bucket->matcher->find_all(text);
      if (local_matches.size() > matches.max_size() - matches.size()) {
        throw std::length_error("incremental Aho match result too large");
      }
      matches.reserve(matches.size() + local_matches.size());
      for (const AhoCorasickMatch& local : local_matches) {
        if (local.pattern_index >= bucket->pattern_ids.size()) {
          throw std::logic_error(
              "incremental Aho local pattern index invariant violated");
        }
        matches.push_back(IncrementalAhoCorasickMatch{
            bucket->pattern_ids[local.pattern_index], local.begin, local.end});
      }
    }

    std::sort(matches.begin(), matches.end(), match_less);
    return matches;
  }

  [[nodiscard]] bool valid_invariants() const noexcept {
    try {
      std::vector<unsigned char> seen(next_pattern_id_, 0U);
      std::size_t represented = 0U;
      std::size_t expected_bucket_size = 1U;

      for (std::size_t level = 0U; level < buckets_.size(); ++level) {
        const auto& bucket = buckets_[level];
        if (bucket.has_value()) {
          if (!bucket->matcher ||
              bucket->pattern_ids.size() != expected_bucket_size ||
              bucket->patterns.size() != expected_bucket_size ||
              bucket->matcher->pattern_count() != expected_bucket_size) {
            return false;
          }

          for (std::size_t index = 0U; index < expected_bucket_size; ++index) {
            const std::size_t pattern_id = bucket->pattern_ids[index];
            if (pattern_id >= next_pattern_id_ || seen[pattern_id] != 0U ||
                (index > 0U &&
                 bucket->pattern_ids[index - 1U] >= pattern_id)) {
              return false;
            }
            seen[pattern_id] = 1U;
          }

          if (expected_bucket_size >
              std::numeric_limits<std::size_t>::max() - represented) {
            return false;
          }
          represented += expected_bucket_size;
        }

        if (level + 1U < buckets_.size()) {
          if (expected_bucket_size >
              std::numeric_limits<std::size_t>::max() / 2U) {
            return false;
          }
          expected_bucket_size *= 2U;
        }
      }

      if (represented != next_pattern_id_) {
        return false;
      }
      for (const unsigned char flag : seen) {
        if (flag == 0U) {
          return false;
        }
      }
      return true;
    } catch (...) {
      return false;
    }
  }

 private:
  struct Bucket {
    std::vector<std::size_t> pattern_ids;
    std::vector<std::string> patterns;
    std::unique_ptr<AhoCorasickByteMatcher> matcher;
  };

  [[nodiscard]] static bool match_less(
      const IncrementalAhoCorasickMatch& first,
      const IncrementalAhoCorasickMatch& second) noexcept {
    if (first.end != second.end) {
      return first.end < second.end;
    }
    if (first.begin != second.begin) {
      return first.begin < second.begin;
    }
    return first.pattern_id < second.pattern_id;
  }

  std::vector<std::optional<Bucket>> buckets_;
  std::size_t next_pattern_id_{};
};

}  // namespace algorithms::strings
