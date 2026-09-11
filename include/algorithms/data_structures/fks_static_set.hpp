#pragma once

#include "algorithms/number_theory/modular.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

struct FksAffineHashParameters {
  std::uint64_t multiplier{};
  std::uint64_t offset{};

  friend bool operator==(const FksAffineHashParameters&,
                         const FksAffineHashParameters&) = default;
};

class FksStaticSet32 {
 public:
  static constexpr std::uint64_t kPrime = 4294967311ULL;

  FksStaticSet32(std::vector<std::uint32_t> keys, std::uint64_t seed,
                 std::size_t max_primary_attempts = 128U,
                 std::size_t max_secondary_attempts = 128U)
      : seed_(seed) {
    if (max_primary_attempts == 0U || max_secondary_attempts == 0U) {
      throw std::invalid_argument("FKS attempt budgets must be positive");
    }

    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    size_ = keys.size();
    if (size_ == 0U) {
      return;
    }
    if (size_ > std::numeric_limits<std::size_t>::max() / 4U) {
      throw std::length_error("FKS key set too large for storage bound");
    }
    if (size_ > std::numeric_limits<std::uint64_t>::max()) {
      throw std::length_error("FKS key set exceeds uint64 hash-index domain");
    }

    SplitMix64 stream(seed_);
    const std::size_t storage_limit = size_ * 4U;
    std::vector<std::size_t> counts(size_, 0U);
    bool primary_found = false;
    for (std::size_t attempt = 0U; attempt < max_primary_attempts; ++attempt) {
      const FksAffineHashParameters candidate = sample_hash(stream);
      std::fill(counts.begin(), counts.end(), 0U);
      for (const std::uint32_t key : keys) {
        ++counts[hash_index(candidate, key, size_)];
      }
      std::size_t square_sum = 0U;
      bool acceptable = true;
      for (const std::size_t count : counts) {
        if (count != 0U && count > storage_limit / count) {
          acceptable = false;
          break;
        }
        const std::size_t square = count * count;
        if (square_sum > storage_limit - square) {
          acceptable = false;
          break;
        }
        square_sum += square;
      }
      if (acceptable) {
        primary_hash_ = candidate;
        primary_attempts_ = attempt + 1U;
        secondary_slot_count_ = square_sum;
        primary_found = true;
        break;
      }
    }
    if (!primary_found) {
      throw std::runtime_error("FKS primary hash attempt budget exhausted");
    }

    std::vector<std::vector<std::uint32_t>> grouped(size_);
    for (std::size_t bucket = 0U; bucket < size_; ++bucket) {
      grouped[bucket].reserve(counts[bucket]);
    }
    for (const std::uint32_t key : keys) {
      grouped[hash_index(primary_hash_, key, size_)].push_back(key);
    }

    buckets_.resize(size_);
    secondary_hashes_.resize(size_);
    for (std::size_t bucket_index = 0U; bucket_index < size_; ++bucket_index) {
      const auto& bucket_keys = grouped[bucket_index];
      const std::size_t count = bucket_keys.size();
      if (count == 0U) {
        continue;
      }
      if (count == 1U) {
        secondary_hashes_[bucket_index] = FksAffineHashParameters{1U, 0U};
        buckets_[bucket_index].resize(1U);
        buckets_[bucket_index][0U] = bucket_keys[0U];
        continue;
      }

      const std::size_t table_size = count * count;
      auto& slots = buckets_[bucket_index];
      slots.resize(table_size);
      bool secondary_found = false;
      for (std::size_t attempt = 0U; attempt < max_secondary_attempts;
           ++attempt) {
        if (secondary_attempts_ == std::numeric_limits<std::size_t>::max()) {
          throw std::overflow_error("FKS secondary attempt counter overflow");
        }
        ++secondary_attempts_;
        const FksAffineHashParameters candidate = sample_hash(stream);
        std::fill(slots.begin(), slots.end(), std::nullopt);
        bool collision = false;
        for (const std::uint32_t key : bucket_keys) {
          const std::size_t slot = hash_index(candidate, key, table_size);
          if (slots[slot].has_value()) {
            collision = true;
            break;
          }
          slots[slot] = key;
        }
        if (!collision) {
          secondary_hashes_[bucket_index] = candidate;
          secondary_found = true;
          break;
        }
      }
      if (!secondary_found) {
        throw std::runtime_error("FKS secondary hash attempt budget exhausted");
      }
    }
  }

  [[nodiscard]] bool contains(std::uint32_t key) const {
    if (size_ == 0U) {
      return false;
    }
    const std::size_t bucket_index = hash_index(primary_hash_, key, size_);
    const auto& slots = buckets_[bucket_index];
    if (slots.empty()) {
      return false;
    }
    const std::size_t slot =
        hash_index(secondary_hashes_[bucket_index], key, slots.size());
    return slots[slot].has_value() && slots[slot].value() == key;
  }

  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] bool empty() const noexcept { return size_ == 0U; }
  [[nodiscard]] std::uint64_t seed() const noexcept { return seed_; }
  [[nodiscard]] std::size_t primary_attempts() const noexcept {
    return primary_attempts_;
  }
  [[nodiscard]] std::size_t secondary_attempts() const noexcept {
    return secondary_attempts_;
  }
  [[nodiscard]] std::size_t secondary_slot_count() const noexcept {
    return secondary_slot_count_;
  }
  [[nodiscard]] FksAffineHashParameters primary_hash_parameters() const noexcept {
    return primary_hash_;
  }
  [[nodiscard]] const std::vector<FksAffineHashParameters>&
  secondary_hash_parameters() const noexcept {
    return secondary_hashes_;
  }

  [[nodiscard]] bool valid_structure() const {
    if (size_ == 0U) {
      return buckets_.empty() && secondary_hashes_.empty() &&
             secondary_slot_count_ == 0U && primary_attempts_ == 0U;
    }
    if (buckets_.size() != size_ || secondary_hashes_.size() != size_ ||
        primary_hash_.multiplier == 0U ||
        primary_hash_.multiplier >= kPrime || primary_hash_.offset >= kPrime ||
        secondary_slot_count_ > size_ * 4U) {
      return false;
    }

    std::set<std::uint32_t> observed;
    std::size_t counted_slots = 0U;
    std::size_t occupied = 0U;
    for (std::size_t bucket_index = 0U; bucket_index < size_; ++bucket_index) {
      const auto& slots = buckets_[bucket_index];
      if (counted_slots > secondary_slot_count_ ||
          slots.size() > secondary_slot_count_ - counted_slots) {
        return false;
      }
      counted_slots += slots.size();
      std::size_t bucket_occupied = 0U;
      for (std::size_t slot = 0U; slot < slots.size(); ++slot) {
        if (!slots[slot].has_value()) {
          continue;
        }
        const std::uint32_t key = slots[slot].value();
        if (hash_index(primary_hash_, key, size_) != bucket_index ||
            hash_index(secondary_hashes_[bucket_index], key, slots.size()) != slot ||
            !observed.insert(key).second) {
          return false;
        }
        ++bucket_occupied;
        ++occupied;
      }
      if (bucket_occupied == 0U) {
        if (!slots.empty()) {
          return false;
        }
        continue;
      }
      if (secondary_hashes_[bucket_index].multiplier == 0U ||
          secondary_hashes_[bucket_index].multiplier >= kPrime ||
          secondary_hashes_[bucket_index].offset >= kPrime ||
          bucket_occupied > std::numeric_limits<std::size_t>::max() /
                                bucket_occupied ||
          slots.size() != bucket_occupied * bucket_occupied) {
        return false;
      }
    }
    if (counted_slots != secondary_slot_count_ || occupied != size_) {
      return false;
    }
    for (const std::uint32_t key : observed) {
      if (!contains(key)) {
        return false;
      }
    }
    return true;
  }

 private:
  class SplitMix64 {
   public:
    explicit SplitMix64(std::uint64_t seed) : state_(seed) {}

    [[nodiscard]] std::uint64_t next() noexcept {
      state_ += 0x9E3779B97F4A7C15ULL;
      std::uint64_t value = state_;
      value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
      value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
      return value ^ (value >> 31U);
    }

   private:
    std::uint64_t state_;
  };

  [[nodiscard]] static std::uint64_t sample_bounded(SplitMix64& stream,
                                                     std::uint64_t bound) {
    const std::uint64_t threshold = (0U - bound) % bound;
    for (;;) {
      const std::uint64_t value = stream.next();
      if (value >= threshold) {
        return value % bound;
      }
    }
  }

  [[nodiscard]] static FksAffineHashParameters sample_hash(SplitMix64& stream) {
    return FksAffineHashParameters{1U + sample_bounded(stream, kPrime - 1U),
                                   sample_bounded(stream, kPrime)};
  }

  [[nodiscard]] static std::size_t hash_index(
      const FksAffineHashParameters& parameters, std::uint32_t key,
      std::size_t table_size) {
    if (table_size == 0U) {
      throw std::logic_error("FKS hash requested for empty table");
    }
    if (table_size > std::numeric_limits<std::uint64_t>::max()) {
      throw std::length_error("FKS table exceeds uint64 index domain");
    }
    const std::uint64_t product = algorithms::number_theory::multiply_mod(
        parameters.multiplier, static_cast<std::uint64_t>(key), kPrime);
    const std::uint64_t affine = (product + parameters.offset) % kPrime;
    return static_cast<std::size_t>(
        affine % static_cast<std::uint64_t>(table_size));
  }

  std::uint64_t seed_{};
  std::size_t size_{};
  std::size_t primary_attempts_{};
  std::size_t secondary_attempts_{};
  std::size_t secondary_slot_count_{};
  FksAffineHashParameters primary_hash_{};
  std::vector<FksAffineHashParameters> secondary_hashes_;
  std::vector<std::vector<std::optional<std::uint32_t>>> buckets_;
};

}  // namespace algorithms::data_structures
