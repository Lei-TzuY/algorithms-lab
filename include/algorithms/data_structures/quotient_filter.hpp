#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace algorithms::data_structures {

class QuotientFilter64 {
 public:
  struct SlotState {
    std::uint64_t remainder{0};
    bool occupied{false};
    bool continuation{false};
    bool shifted{false};

    friend bool operator==(const SlotState&, const SlotState&) = default;
  };

  QuotientFilter64(std::uint8_t quotient_bits, std::uint8_t remainder_bits,
                   std::uint64_t seed = 0)
      : quotient_bits_(quotient_bits), remainder_bits_(remainder_bits), seed_(seed) {
    if (quotient_bits_ < 2U || quotient_bits_ > 20U) {
      throw std::invalid_argument("QuotientFilter64 quotient_bits must be in [2,20]");
    }
    if (remainder_bits_ == 0U ||
        static_cast<unsigned>(quotient_bits_) + static_cast<unsigned>(remainder_bits_) > 64U) {
      throw std::invalid_argument("QuotientFilter64 fingerprint width must be in [q+1,64]");
    }
    slots_.resize(std::size_t{1} << quotient_bits_);
  }

  [[nodiscard]] bool contains(std::uint64_t value) const {
    const auto [quotient, remainder] = split_fingerprint(value);
    return contains_fingerprint(quotient, remainder);
  }

  bool insert(std::uint64_t value) {
    const auto [quotient, remainder] = split_fingerprint(value);
    if (contains_fingerprint(quotient, remainder)) {
      return false;
    }
    if (size_ >= slots_.size() - 1U) {
      throw std::overflow_error("QuotientFilter64 requires one empty sentinel slot");
    }

    const bool quotient_was_occupied = slots_[quotient].occupied;
    const bool canonical_was_empty = empty_slot(quotient);
    if (!quotient_was_occupied) {
      slots_[quotient].occupied = true;
    }

    if (canonical_was_empty) {
      SlotState& slot = slots_[quotient];
      slot.remainder = remainder;
      slot.continuation = false;
      slot.shifted = false;
      ++size_;
      return true;
    }

    const std::size_t run_start = find_run_start(quotient);
    std::size_t insertion = run_start;
    bool continuation = false;
    bool force_displaced_continuation = false;

    if (quotient_was_occupied) {
      if (remainder < slots_[run_start].remainder) {
        insertion = run_start;
        continuation = false;
        force_displaced_continuation = true;
      } else {
        insertion = run_start;
        while (true) {
          const std::size_t next = increment(insertion);
          if (!slots_[next].continuation || remainder < slots_[next].remainder) {
            insertion = next;
            break;
          }
          insertion = next;
        }
        continuation = true;
      }
    }

    shift_insert(insertion, remainder, continuation, insertion != quotient,
                 force_displaced_continuation);
    ++size_;
    return true;
  }

  void clear() noexcept {
    std::fill(slots_.begin(), slots_.end(), SlotState{});
    size_ = 0;
  }

  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
  [[nodiscard]] std::size_t capacity() const noexcept { return slots_.size() - 1U; }
  [[nodiscard]] std::uint8_t quotient_bits() const noexcept { return quotient_bits_; }
  [[nodiscard]] std::uint8_t remainder_bits() const noexcept { return remainder_bits_; }
  [[nodiscard]] std::uint64_t seed() const noexcept { return seed_; }
  [[nodiscard]] const std::vector<SlotState>& debug_slots() const noexcept { return slots_; }

  [[nodiscard]] bool valid_structure() const {
    if (size_ >= slots_.size()) {
      return false;
    }
    std::size_t physical = 0;
    for (std::size_t i = 0; i < slots_.size(); ++i) {
      if (!empty_slot(i)) {
        ++physical;
      }
    }
    if (physical != size_) {
      return false;
    }

    std::vector<bool> visited(slots_.size(), false);
    std::size_t logical = 0;
    for (std::size_t quotient = 0; quotient < slots_.size(); ++quotient) {
      if (!slots_[quotient].occupied) {
        continue;
      }
      const std::size_t start = find_run_start(quotient);
      std::size_t pos = start;
      bool first = true;
      std::uint64_t previous = 0;
      for (std::size_t steps = 0; steps < slots_.size(); ++steps) {
        const SlotState& slot = slots_[pos];
        if (empty_slot(pos) || visited[pos]) {
          return false;
        }
        if (first) {
          if (slot.continuation) {
            return false;
          }
        } else {
          if (!slot.continuation || slot.remainder <= previous) {
            return false;
          }
        }
        if (slot.shifted != (pos != quotient)) {
          return false;
        }
        visited[pos] = true;
        ++logical;
        previous = slot.remainder;
        first = false;

        const std::size_t next = increment(pos);
        if (!slots_[next].continuation) {
          break;
        }
        pos = next;
      }
    }

    if (logical != size_) {
      return false;
    }
    for (std::size_t i = 0; i < slots_.size(); ++i) {
      if (!empty_slot(i) && !visited[i]) {
        return false;
      }
    }
    return true;
  }

 private:
  [[nodiscard]] std::size_t increment(std::size_t index) const noexcept {
    return (index + 1U) & (slots_.size() - 1U);
  }

  [[nodiscard]] std::size_t decrement(std::size_t index) const noexcept {
    return (index - 1U) & (slots_.size() - 1U);
  }

  [[nodiscard]] bool empty_slot(std::size_t index) const noexcept {
    const SlotState& slot = slots_[index];
    return !slot.occupied && !slot.continuation && !slot.shifted;
  }

  [[nodiscard]] std::size_t find_run_start(std::size_t quotient) const {
    std::size_t bucket = quotient;
    for (std::size_t steps = 0; slots_[bucket].shifted; ++steps) {
      if (steps >= slots_.size()) {
        throw std::logic_error("QuotientFilter64 malformed cluster");
      }
      bucket = decrement(bucket);
    }

    std::size_t run = bucket;
    for (std::size_t guard = 0; bucket != quotient; ++guard) {
      if (guard >= slots_.size()) {
        throw std::logic_error("QuotientFilter64 malformed quotient walk");
      }
      do {
        run = increment(run);
      } while (slots_[run].continuation);

      do {
        bucket = increment(bucket);
      } while (!slots_[bucket].occupied);
    }
    return run;
  }

  [[nodiscard]] bool contains_fingerprint(std::size_t quotient,
                                          std::uint64_t remainder) const {
    if (!slots_[quotient].occupied) {
      return false;
    }
    std::size_t pos = find_run_start(quotient);
    for (std::size_t steps = 0; steps < slots_.size(); ++steps) {
      if (slots_[pos].remainder == remainder) {
        return true;
      }
      if (slots_[pos].remainder > remainder) {
        return false;
      }
      const std::size_t next = increment(pos);
      if (!slots_[next].continuation) {
        return false;
      }
      pos = next;
    }
    throw std::logic_error("QuotientFilter64 malformed run");
  }

  void shift_insert(std::size_t position, std::uint64_t remainder,
                    bool continuation, bool shifted,
                    bool force_displaced_continuation) {
    for (std::size_t steps = 0; steps < slots_.size(); ++steps) {
      SlotState& slot = slots_[position];
      const bool destination_occupied = slot.occupied;
      if (empty_slot(position)) {
        slot.remainder = remainder;
        slot.continuation = continuation;
        slot.shifted = shifted;
        slot.occupied = destination_occupied;
        return;
      }

      std::uint64_t displaced_remainder = slot.remainder;
      bool displaced_continuation = slot.continuation;
      if (force_displaced_continuation) {
        displaced_continuation = true;
        force_displaced_continuation = false;
      }

      slot.remainder = remainder;
      slot.continuation = continuation;
      slot.shifted = shifted;
      slot.occupied = destination_occupied;

      remainder = displaced_remainder;
      continuation = displaced_continuation;
      shifted = true;
      position = increment(position);
    }
    throw std::logic_error("QuotientFilter64 shift failed to find empty slot");
  }

  [[nodiscard]] std::pair<std::size_t, std::uint64_t> split_fingerprint(
      std::uint64_t value) const noexcept {
    const std::uint64_t hash = mix(value, seed_);
    const unsigned fingerprint_bits =
        static_cast<unsigned>(quotient_bits_) + static_cast<unsigned>(remainder_bits_);
    const std::uint64_t fingerprint =
        fingerprint_bits == 64U ? hash : (hash >> (64U - fingerprint_bits));
    const std::uint64_t mask = (UINT64_C(1) << remainder_bits_) - 1U;
    const std::uint64_t remainder = fingerprint & mask;
    const std::size_t quotient =
        static_cast<std::size_t>(fingerprint >> remainder_bits_);
    return {quotient, remainder};
  }

  static std::uint64_t mix(std::uint64_t value, std::uint64_t seed) noexcept {
    std::uint64_t z = value + seed + UINT64_C(0x9e3779b97f4a7c15);
    z = (z ^ (z >> 30U)) * UINT64_C(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27U)) * UINT64_C(0x94d049bb133111eb);
    return z ^ (z >> 31U);
  }

  std::uint8_t quotient_bits_;
  std::uint8_t remainder_bits_;
  std::uint64_t seed_;
  std::vector<SlotState> slots_;
  std::size_t size_{0};
};

}  // namespace algorithms::data_structures
