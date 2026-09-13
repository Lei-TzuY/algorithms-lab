#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

struct CuckooPlacement {
  std::int64_t key;
  std::uint8_t table;
  std::size_t slot;

  friend bool operator==(const CuckooPlacement&, const CuckooPlacement&) = default;
};

class CuckooHashSet {
 public:
  static_assert(std::numeric_limits<std::size_t>::digits <= 64,
                "CuckooHashSet requires size_t no wider than uint64_t");
  explicit CuckooHashSet(std::uint64_t seed, std::size_t initial_capacity = 8)
      : seed_(seed), capacity_(normalize_capacity(initial_capacity)) {
    set_salts(0);
    table0_.resize(capacity_);
    table1_.resize(capacity_);
  }

  [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] std::size_t capacity_per_table() const noexcept { return capacity_; }
  [[nodiscard]] std::size_t rebuild_count() const noexcept { return rebuild_count_; }
  [[nodiscard]] std::uint64_t rehash_generation() const noexcept {
    return rehash_generation_;
  }

  [[nodiscard]] bool contains(std::int64_t key) const noexcept {
    const std::size_t first = bucket(key, salt0_, capacity_);
    if (table0_[first].has_value() && *table0_[first] == key) {
      return true;
    }
    const std::size_t second = bucket(key, salt1_, capacity_);
    return table1_[second].has_value() && *table1_[second] == key;
  }

  bool insert(std::int64_t key) {
    if (contains(key)) {
      return false;
    }
    if (size_ == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error("cuckoo hash set size overflow");
    }

    const std::size_t new_size = size_ + 1;
    if (new_size > capacity_) {
      rebuild_including(key, grow_capacity(capacity_));
      return true;
    }

    if (try_place_current(key)) {
      ++size_;
      return true;
    }

    rebuild_including(key, capacity_);
    return true;
  }

  bool erase(std::int64_t key) noexcept {
    const std::size_t first = bucket(key, salt0_, capacity_);
    if (table0_[first].has_value() && *table0_[first] == key) {
      table0_[first].reset();
      --size_;
      return true;
    }
    const std::size_t second = bucket(key, salt1_, capacity_);
    if (table1_[second].has_value() && *table1_[second] == key) {
      table1_[second].reset();
      --size_;
      return true;
    }
    return false;
  }

  [[nodiscard]] std::vector<std::int64_t> sorted_values() const {
    std::vector<std::int64_t> values;
    values.reserve(size_);
    append_present(table0_, values);
    append_present(table1_, values);
    std::sort(values.begin(), values.end());
    return values;
  }

  [[nodiscard]] std::vector<CuckooPlacement> layout() const {
    std::vector<CuckooPlacement> result;
    result.reserve(size_);
    for (std::size_t index = 0; index < capacity_; ++index) {
      if (table0_[index].has_value()) {
        result.push_back(CuckooPlacement{*table0_[index], 0, index});
      }
      if (table1_[index].has_value()) {
        result.push_back(CuckooPlacement{*table1_[index], 1, index});
      }
    }
    std::sort(result.begin(), result.end(),
              [](const CuckooPlacement& left, const CuckooPlacement& right) {
                return left.key < right.key;
              });
    return result;
  }

  [[nodiscard]] bool valid_structure() const {
    if (capacity_ < kMinimumCapacity || !std::has_single_bit(capacity_) ||
        table0_.size() != capacity_ || table1_.size() != capacity_) {
      return false;
    }

    std::vector<std::int64_t> keys;
    keys.reserve(size_);
    for (std::size_t index = 0; index < capacity_; ++index) {
      if (table0_[index].has_value()) {
        const std::int64_t key = *table0_[index];
        if (bucket(key, salt0_, capacity_) != index) {
          return false;
        }
        keys.push_back(key);
      }
      if (table1_[index].has_value()) {
        const std::int64_t key = *table1_[index];
        if (bucket(key, salt1_, capacity_) != index) {
          return false;
        }
        keys.push_back(key);
      }
    }
    if (keys.size() != size_) {
      return false;
    }
    std::sort(keys.begin(), keys.end());
    return std::adjacent_find(keys.begin(), keys.end()) == keys.end();
  }

 private:
  static constexpr std::size_t kMinimumCapacity = 4;
  static constexpr std::size_t kRehashAttempts = 32;
  static constexpr std::size_t kAttemptsPerCapacity = 8;

  struct Change {
    std::uint8_t table;
    std::size_t slot;
    std::optional<std::int64_t> previous;
  };

  static std::uint64_t mix64(std::uint64_t value) noexcept {
    value += 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
  }

  static std::uint64_t derive_salt(std::uint64_t seed, std::uint64_t generation,
                                   std::uint64_t lane) noexcept {
    return mix64(seed ^ mix64(generation + 0xD1B54A32D192ED03ULL * lane));
  }

  void set_salts(std::uint64_t generation) noexcept {
    rehash_generation_ = generation;
    salt0_ = derive_salt(seed_, generation, 1);
    salt1_ = derive_salt(seed_, generation, 2);
    if (salt0_ == salt1_) {
      salt1_ ^= 0xA0761D6478BD642FULL;
    }
  }

  static std::size_t bucket(std::int64_t key, std::uint64_t salt,
                            std::size_t capacity) noexcept {
    const std::uint64_t bits = static_cast<std::uint64_t>(key);
    const std::uint64_t mixed = mix64(bits ^ salt);
    return static_cast<std::size_t>(mixed & static_cast<std::uint64_t>(capacity - 1));
  }

  static std::size_t normalize_capacity(std::size_t requested) {
    std::size_t capacity = std::max(requested, kMinimumCapacity);
    if (std::has_single_bit(capacity)) {
      return capacity;
    }
    const std::size_t width = static_cast<std::size_t>(std::bit_width(capacity));
    if (width >= static_cast<std::size_t>(std::numeric_limits<std::size_t>::digits)) {
      throw std::length_error("cuckoo hash capacity is not representable");
    }
    return std::size_t{1} << width;
  }

  static std::size_t grow_capacity(std::size_t capacity) {
    if (capacity > std::numeric_limits<std::size_t>::max() / 2) {
      throw std::length_error("cuckoo hash capacity overflow");
    }
    return capacity * 2;
  }

  static std::size_t kick_budget(std::size_t capacity) noexcept {
    const std::size_t width = static_cast<std::size_t>(std::bit_width(capacity));
    return 8 * width + 32;
  }

  static void append_present(const std::vector<std::optional<std::int64_t>>& table,
                             std::vector<std::int64_t>& output) {
    for (const auto& value : table) {
      if (value.has_value()) {
        output.push_back(*value);
      }
    }
  }

  bool try_place_current(std::int64_t key) {
    std::optional<std::int64_t> pending = key;
    std::uint8_t table_id = 0;
    std::vector<Change> changes;
    changes.reserve(kick_budget(capacity_));

    for (std::size_t kick = 0; kick < kick_budget(capacity_); ++kick) {
      auto& table = table_id == 0 ? table0_ : table1_;
      const std::uint64_t salt = table_id == 0 ? salt0_ : salt1_;
      const std::size_t index = bucket(*pending, salt, capacity_);
      changes.push_back(Change{table_id, index, table[index]});
      std::swap(pending, table[index]);
      if (!pending.has_value()) {
        return true;
      }
      table_id ^= 1U;
    }

    for (auto iterator = changes.rbegin(); iterator != changes.rend(); ++iterator) {
      auto& table = iterator->table == 0 ? table0_ : table1_;
      table[iterator->slot] = iterator->previous;
    }
    return false;
  }

  static bool try_place_temporary(
      std::vector<std::optional<std::int64_t>>& table0,
      std::vector<std::optional<std::int64_t>>& table1, std::int64_t key,
      std::uint64_t salt0, std::uint64_t salt1, std::size_t capacity) {
    std::optional<std::int64_t> pending = key;
    std::uint8_t table_id = 0;
    for (std::size_t kick = 0; kick < kick_budget(capacity); ++kick) {
      auto& table = table_id == 0 ? table0 : table1;
      const std::uint64_t salt = table_id == 0 ? salt0 : salt1;
      const std::size_t index = bucket(*pending, salt, capacity);
      std::swap(pending, table[index]);
      if (!pending.has_value()) {
        return true;
      }
      table_id ^= 1U;
    }
    return false;
  }

  void rebuild_including(std::int64_t new_key, std::size_t initial_capacity) {
    std::vector<std::int64_t> keys = sorted_values();
    keys.push_back(new_key);

    std::size_t candidate_capacity = initial_capacity;
    std::uint64_t candidate_generation = rehash_generation_;
    for (std::size_t attempt = 0; attempt < kRehashAttempts; ++attempt) {
      ++candidate_generation;
      std::uint64_t candidate_salt0 = derive_salt(seed_, candidate_generation, 1);
      std::uint64_t candidate_salt1 = derive_salt(seed_, candidate_generation, 2);
      if (candidate_salt0 == candidate_salt1) {
        candidate_salt1 ^= 0xA0761D6478BD642FULL;
      }

      std::vector<std::optional<std::int64_t>> candidate0(candidate_capacity);
      std::vector<std::optional<std::int64_t>> candidate1(candidate_capacity);
      bool success = true;
      for (const std::int64_t key : keys) {
        if (!try_place_temporary(candidate0, candidate1, key, candidate_salt0,
                                 candidate_salt1, candidate_capacity)) {
          success = false;
          break;
        }
      }
      if (success) {
        table0_ = std::move(candidate0);
        table1_ = std::move(candidate1);
        capacity_ = candidate_capacity;
        size_ = keys.size();
        rehash_generation_ = candidate_generation;
        salt0_ = candidate_salt0;
        salt1_ = candidate_salt1;
        ++rebuild_count_;
        return;
      }

      if (attempt + 1 < kRehashAttempts &&
          (attempt + 1) % kAttemptsPerCapacity == 0) {
        candidate_capacity = grow_capacity(candidate_capacity);
      }
    }

    throw std::runtime_error("cuckoo hash rebuild attempt budget exhausted");
  }

  std::uint64_t seed_;
  std::size_t capacity_;
  std::size_t size_ = 0;
  std::uint64_t rehash_generation_ = 0;
  std::uint64_t salt0_ = 0;
  std::uint64_t salt1_ = 0;
  std::size_t rebuild_count_ = 0;
  std::vector<std::optional<std::int64_t>> table0_;
  std::vector<std::optional<std::int64_t>> table1_;
};

}  // namespace algorithms::data_structures
