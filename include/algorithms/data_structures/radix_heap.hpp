#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

template <typename T>
class RadixHeap {
 public:
  using Key = std::uint64_t;
  struct Entry { Key key{}; T value; };

  [[nodiscard]] bool empty() const noexcept { return size_ == 0U; }
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] Key last_popped_key() const noexcept { return last_; }
  [[nodiscard]] std::size_t redistribution_count() const noexcept { return redistributions_; }

  void push(Key key, const T& value) { emplace_entry(key, value); }
  void push(Key key, T&& value) { emplace_entry(key, std::move(value)); }

  Entry pop() {
    if (empty()) throw std::out_of_range("RadixHeap::pop on empty heap");
    if (buckets_[0].empty()) redistribute_lowest_bucket();
    Entry result = std::move(buckets_[0].back());
    buckets_[0].pop_back();
    --size_;
    return result;
  }

  [[nodiscard]] bool valid_structure() const noexcept {
    std::size_t counted = 0U;
    for (std::size_t bucket = 0; bucket < kBucketCount; ++bucket) {
      counted += buckets_[bucket].size();
      for (const auto& entry : buckets_[bucket]) {
        if (entry.key < last_ || bucket_index(entry.key, last_) != bucket) return false;
      }
    }
    return counted == size_;
  }

 private:
  static constexpr std::size_t kBucketCount =
      static_cast<std::size_t>(std::numeric_limits<Key>::digits) + 1U;

  [[nodiscard]] static std::size_t bucket_index(Key key, Key last) noexcept {
    if (key == last) return 0U;
    return static_cast<std::size_t>(std::bit_width(key ^ last));
  }

  template <typename U>
  void emplace_entry(Key key, U&& value) {
    if (key < last_) throw std::invalid_argument("RadixHeap requires monotone keys");
    buckets_[bucket_index(key, last_)].push_back(Entry{key, std::forward<U>(value)});
    ++size_;
  }

  void redistribute_lowest_bucket() {
    std::size_t bucket = 1U;
    while (bucket < kBucketCount && buckets_[bucket].empty()) ++bucket;
    if (bucket == kBucketCount) throw std::logic_error("RadixHeap size invariant broken");

    Key new_last = buckets_[bucket].front().key;
    for (const auto& entry : buckets_[bucket]) {
      if (entry.key < new_last) new_last = entry.key;
    }
    last_ = new_last;

    std::vector<Entry> moved;
    moved.swap(buckets_[bucket]);
    ++redistributions_;
    for (auto& entry : moved) {
      buckets_[bucket_index(entry.key, last_)].push_back(std::move(entry));
    }
  }

  std::array<std::vector<Entry>, kBucketCount> buckets_{};
  Key last_{};
  std::size_t size_{};
  std::size_t redistributions_{};
};

}  // namespace algorithms::data_structures
