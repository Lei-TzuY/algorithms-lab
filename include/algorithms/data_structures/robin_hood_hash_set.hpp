#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

struct RobinHoodSlotDebug {
  bool occupied;
  std::int64_t key;
  std::size_t home_index;
  std::size_t probe_distance;

  friend bool operator==(const RobinHoodSlotDebug&, const RobinHoodSlotDebug&) =
      default;
};

class RobinHoodHashSet64 {
 public:
  explicit RobinHoodHashSet64(std::size_t initial_capacity = 8,
                              std::uint64_t seed = 0)
      : slots_(normalize_capacity(initial_capacity)), seed_(seed) {}

  [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] std::size_t capacity() const noexcept { return slots_.size(); }
  [[nodiscard]] std::uint64_t seed() const noexcept { return seed_; }

  [[nodiscard]] bool contains(std::int64_t key) const noexcept {
    return find_index(key) != npos();
  }

  bool insert(std::int64_t key) {
    if (contains(key)) {
      return false;
    }
    ensure_insert_capacity();
    insert_no_grow(slots_, seed_, key);
    ++size_;
    return true;
  }

  bool erase(std::int64_t key) noexcept {
    const std::size_t found = find_index(key);
    if (found == npos()) {
      return false;
    }

    const std::size_t mask = slots_.size() - 1;
    std::size_t hole = found;
    std::size_t next = (hole + 1) & mask;
    while (slots_[next].occupied && slots_[next].probe_distance != 0) {
      slots_[hole] = slots_[next];
      --slots_[hole].probe_distance;
      hole = next;
      next = (next + 1) & mask;
    }
    slots_[hole] = Slot{};
    --size_;
    return true;
  }

  [[nodiscard]] std::vector<std::int64_t> values_sorted() const {
    std::vector<std::int64_t> values;
    values.reserve(size_);
    for (const Slot& slot : slots_) {
      if (slot.occupied) {
        values.push_back(slot.key);
      }
    }
    std::sort(values.begin(), values.end());
    return values;
  }

  [[nodiscard]] std::vector<RobinHoodSlotDebug> debug_slots() const {
    std::vector<RobinHoodSlotDebug> result;
    result.reserve(slots_.size());
    for (const Slot& slot : slots_) {
      result.push_back(RobinHoodSlotDebug{
          slot.occupied, slot.key,
          slot.occupied ? home_index(slot.key, slots_.size()) : 0,
          slot.occupied ? slot.probe_distance : 0});
    }
    return result;
  }

  [[nodiscard]] bool valid_structure() const {
    const std::size_t cap = slots_.size();
    if (cap < 8 || (cap & (cap - 1)) != 0) {
      return false;
    }
    if (size_ > max_size_before_growth(cap) || size_ >= cap) {
      return false;
    }

    const std::size_t mask = cap - 1;
    std::size_t occupied_count = 0;
    std::vector<std::int64_t> keys;
    keys.reserve(size_);

    for (std::size_t index = 0; index < cap; ++index) {
      const Slot& slot = slots_[index];
      if (!slot.occupied) {
        if (slot.probe_distance != 0) {
          return false;
        }
        continue;
      }

      ++occupied_count;
      keys.push_back(slot.key);
      const std::size_t home = home_index(slot.key, cap);
      const std::size_t expected_distance = (index - home) & mask;
      if (slot.probe_distance != expected_distance || expected_distance >= cap) {
        return false;
      }

      std::size_t cursor = home;
      for (std::size_t distance = 0; distance < expected_distance; ++distance) {
        if (!slots_[cursor].occupied) {
          return false;
        }
        cursor = (cursor + 1) & mask;
      }
      if (find_index(slot.key) != index) {
        return false;
      }
    }

    if (occupied_count != size_) {
      return false;
    }
    std::sort(keys.begin(), keys.end());
    if (std::adjacent_find(keys.begin(), keys.end()) != keys.end()) {
      return false;
    }
    return true;
  }

 private:
  struct Slot {
    bool occupied = false;
    std::int64_t key = 0;
    std::size_t probe_distance = 0;
  };

  std::vector<Slot> slots_;
  std::size_t size_ = 0;
  std::uint64_t seed_ = 0;

  [[nodiscard]] static constexpr std::size_t npos() noexcept {
    return std::numeric_limits<std::size_t>::max();
  }

  [[nodiscard]] static std::size_t normalize_capacity(std::size_t requested) {
    std::size_t capacity = 8;
    while (capacity < requested) {
      if (capacity > std::numeric_limits<std::size_t>::max() / 2) {
        throw std::length_error("Robin Hood hash capacity is too large");
      }
      capacity *= 2;
    }
    return capacity;
  }

  [[nodiscard]] static std::size_t max_size_before_growth(
      std::size_t capacity) noexcept {
    return capacity - (capacity / 8);
  }

  [[nodiscard]] static std::uint64_t mix(std::uint64_t value,
                                         std::uint64_t seed) noexcept {
    std::uint64_t z = value + seed + 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27U)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31U);
  }

  [[nodiscard]] static std::size_t home_index(std::int64_t key,
                                               std::size_t capacity,
                                               std::uint64_t seed) noexcept {
    const std::uint64_t bits = static_cast<std::uint64_t>(key);
    return static_cast<std::size_t>(mix(bits, seed)) & (capacity - 1);
  }

  [[nodiscard]] std::size_t home_index(std::int64_t key,
                                       std::size_t capacity) const noexcept {
    return home_index(key, capacity, seed_);
  }

  [[nodiscard]] std::size_t find_index(std::int64_t key) const noexcept {
    const std::size_t cap = slots_.size();
    const std::size_t mask = cap - 1;
    std::size_t index = home_index(key, cap);
    std::size_t distance = 0;

    while (distance < cap) {
      const Slot& slot = slots_[index];
      if (!slot.occupied || slot.probe_distance < distance) {
        return npos();
      }
      if (slot.key == key) {
        return index;
      }
      index = (index + 1) & mask;
      ++distance;
    }
    return npos();
  }

  static void insert_no_grow(std::vector<Slot>& slots, std::uint64_t seed,
                             std::int64_t key) {
    const std::size_t cap = slots.size();
    const std::size_t mask = cap - 1;
    Slot incoming{true, key, 0};
    std::size_t index = home_index(key, cap, seed);

    for (std::size_t probes = 0; probes < cap; ++probes) {
      Slot& resident = slots[index];
      if (!resident.occupied) {
        resident = incoming;
        return;
      }
      if (resident.probe_distance < incoming.probe_distance) {
        std::swap(resident, incoming);
      }
      index = (index + 1) & mask;
      ++incoming.probe_distance;
    }
    throw std::logic_error("Robin Hood insertion found no empty slot");
  }

  void ensure_insert_capacity() {
    if (size_ < max_size_before_growth(slots_.size())) {
      return;
    }
    if (slots_.size() > std::numeric_limits<std::size_t>::max() / 2) {
      throw std::length_error("Robin Hood hash capacity cannot grow");
    }

    const std::size_t new_capacity = slots_.size() * 2;
    std::vector<Slot> grown(new_capacity);
    for (const Slot& slot : slots_) {
      if (slot.occupied) {
        insert_no_grow(grown, seed_, slot.key);
      }
    }
    slots_.swap(grown);
  }
};

}  // namespace algorithms::data_structures
