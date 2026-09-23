#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <variant>
#include <vector>

namespace algorithms::data_structures {

// Exact mutable uint32 set using a Roaring-style two-level layout.
//
// The high 16 bits select a container. Sparse containers store sorted uint16
// lows. Once a container exceeds 4096 values it becomes a 65,536-bit bitmap;
// erasing back to 4096 values converts it to the sparse representation again.
//
// This class intentionally does not claim Roaring serialization compatibility,
// run containers, SIMD acceleration, or benchmark-backed performance.
class RoaringStyleBitmap32 {
 public:
  using Value = std::uint32_t;

  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] bool empty() const noexcept { return size_ == 0U; }

  [[nodiscard]] std::size_t container_count() const noexcept {
    return containers_.size();
  }

  [[nodiscard]] std::size_t array_container_count() const noexcept {
    std::size_t count = 0U;
    for (const Container& container : containers_) {
      if (container.is_array()) {
        ++count;
      }
    }
    return count;
  }

  [[nodiscard]] std::size_t bitmap_container_count() const noexcept {
    return containers_.size() - array_container_count();
  }

  [[nodiscard]] bool contains(const Value value) const noexcept {
    const std::uint16_t high = high_bits(value);
    const std::uint16_t low = low_bits(value);
    const auto it = lower_bound_container(high);
    if (it == containers_.end() || it->high != high) {
      return false;
    }
    return contains_low(*it, low);
  }

  // Returns true exactly when the set changed.
  bool insert(const Value value) {
    if (size_ == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error("Roaring-style bitmap cardinality exhausted");
    }

    const std::uint16_t high = high_bits(value);
    const std::uint16_t low = low_bits(value);
    auto it = lower_bound_container(high);

    if (it == containers_.end() || it->high != high) {
      Container fresh(high, low);
      containers_.insert(it, std::move(fresh));
      ++size_;
      return true;
    }

    if (it->is_array()) {
      auto& values = std::get<ArrayPayload>(it->payload);
      const auto position =
          std::lower_bound(values.begin(), values.end(), low);
      if (position != values.end() && *position == low) {
        return false;
      }
      values.insert(position, low);
      ++it->cardinality;
      ++size_;
      if (it->cardinality > kArrayLimit) {
        convert_to_bitmap(*it);
      }
      return true;
    }

    auto& words = std::get<BitmapPayload>(it->payload);
    const std::size_t word_index = static_cast<std::size_t>(low) / 64U;
    const std::size_t bit_index = static_cast<std::size_t>(low) % 64U;
    const std::uint64_t mask = UINT64_C(1) << bit_index;
    if ((words[word_index] & mask) != 0U) {
      return false;
    }
    words[word_index] |= mask;
    ++it->cardinality;
    ++size_;
    return true;
  }

  // Returns true exactly when the set changed.
  bool erase(const Value value) {
    const std::uint16_t high = high_bits(value);
    const std::uint16_t low = low_bits(value);
    auto it = lower_bound_container(high);
    if (it == containers_.end() || it->high != high) {
      return false;
    }

    if (it->is_array()) {
      auto& values = std::get<ArrayPayload>(it->payload);
      const auto position =
          std::lower_bound(values.begin(), values.end(), low);
      if (position == values.end() || *position != low) {
        return false;
      }
      values.erase(position);
      --it->cardinality;
      --size_;
      if (it->cardinality == 0U) {
        containers_.erase(it);
      }
      return true;
    }

    auto& words = std::get<BitmapPayload>(it->payload);
    const std::size_t word_index = static_cast<std::size_t>(low) / 64U;
    const std::size_t bit_index = static_cast<std::size_t>(low) % 64U;
    const std::uint64_t mask = UINT64_C(1) << bit_index;
    if ((words[word_index] & mask) == 0U) {
      return false;
    }

    words[word_index] &= ~mask;
    --it->cardinality;
    --size_;

    if (it->cardinality == 0U) {
      containers_.erase(it);
    } else if (it->cardinality <= kArrayLimit) {
      convert_to_array(*it);
    }
    return true;
  }

  // Number of stored values <= value.
  [[nodiscard]] std::size_t rank(const Value value) const noexcept {
    const std::uint16_t high = high_bits(value);
    const std::uint16_t low = low_bits(value);
    const auto it = lower_bound_container(high);

    std::size_t total = 0U;
    for (auto current = containers_.begin(); current != it; ++current) {
      total += current->cardinality;
    }

    if (it != containers_.end() && it->high == high) {
      total += rank_low(*it, low);
    }
    return total;
  }

  // Zero-based kth smallest stored value.
  [[nodiscard]] std::optional<Value> select(
      std::size_t index) const noexcept {
    if (index >= size_) {
      return std::nullopt;
    }

    for (const Container& container : containers_) {
      if (index >= container.cardinality) {
        index -= container.cardinality;
        continue;
      }

      const std::uint16_t low = select_low(container, index);
      return (static_cast<Value>(container.high) << 16U) |
             static_cast<Value>(low);
    }

    return std::nullopt;
  }

  // Expensive invariant replay, excluded from operation-complexity claims.
  [[nodiscard]] bool valid_structure() const noexcept {
    try {
      std::size_t total = 0U;
      std::optional<std::uint16_t> previous_high;

      for (const Container& container : containers_) {
        if (previous_high.has_value() &&
            container.high <= *previous_high) {
          return false;
        }
        previous_high = container.high;

        if (container.cardinality == 0U) {
          return false;
        }
        if (total > std::numeric_limits<std::size_t>::max() -
                        container.cardinality) {
          return false;
        }
        total += container.cardinality;

        if (container.is_array()) {
          const auto& values =
              std::get<ArrayPayload>(container.payload);
          if (values.size() != container.cardinality ||
              values.size() > kArrayLimit ||
              !std::is_sorted(values.begin(), values.end()) ||
              std::adjacent_find(values.begin(), values.end()) !=
                  values.end()) {
            return false;
          }
          continue;
        }

        const auto& words =
            std::get<BitmapPayload>(container.payload);
        if (words.size() != kBitmapWords ||
            container.cardinality <= kArrayLimit) {
          return false;
        }

        std::size_t counted = 0U;
        for (const std::uint64_t word : words) {
          counted += static_cast<std::size_t>(std::popcount(word));
        }
        if (counted != container.cardinality) {
          return false;
        }
      }

      return total == size_;
    } catch (...) {
      return false;
    }
  }

 private:
  static constexpr std::size_t kArrayLimit = 4096U;
  static constexpr std::size_t kBitmapWords = 65536U / 64U;

  using ArrayPayload = std::vector<std::uint16_t>;
  using BitmapPayload = std::vector<std::uint64_t>;

  struct Container {
    std::uint16_t high{};
    std::size_t cardinality{};
    std::variant<ArrayPayload, BitmapPayload> payload;

    Container(const std::uint16_t high_bits,
              const std::uint16_t low_bits)
        : high(high_bits),
          cardinality(1U),
          payload(ArrayPayload{low_bits}) {}

    [[nodiscard]] bool is_array() const noexcept {
      return std::holds_alternative<ArrayPayload>(payload);
    }
  };

  std::vector<Container> containers_;
  std::size_t size_{};

  [[nodiscard]] static constexpr std::uint16_t high_bits(
      const Value value) noexcept {
    return static_cast<std::uint16_t>(value >> 16U);
  }

  [[nodiscard]] static constexpr std::uint16_t low_bits(
      const Value value) noexcept {
    return static_cast<std::uint16_t>(value & UINT32_C(0xffff));
  }

  [[nodiscard]] auto lower_bound_container(
      const std::uint16_t high) noexcept {
    return std::lower_bound(
        containers_.begin(), containers_.end(), high,
        [](const Container& container, const std::uint16_t key) {
          return container.high < key;
        });
  }

  [[nodiscard]] auto lower_bound_container(
      const std::uint16_t high) const noexcept {
    return std::lower_bound(
        containers_.begin(), containers_.end(), high,
        [](const Container& container, const std::uint16_t key) {
          return container.high < key;
        });
  }

  [[nodiscard]] static bool contains_low(
      const Container& container,
      const std::uint16_t low) noexcept {
    if (container.is_array()) {
      const auto& values =
          std::get<ArrayPayload>(container.payload);
      return std::binary_search(values.begin(), values.end(), low);
    }

    const auto& words =
        std::get<BitmapPayload>(container.payload);
    const std::size_t word_index = static_cast<std::size_t>(low) / 64U;
    const std::size_t bit_index = static_cast<std::size_t>(low) % 64U;
    return (words[word_index] &
            (UINT64_C(1) << bit_index)) != 0U;
  }

  static void convert_to_bitmap(Container& container) {
    if (!container.is_array()) {
      throw std::logic_error(
          "Roaring-style bitmap conversion requires array payload");
    }

    const auto& values =
        std::get<ArrayPayload>(container.payload);
    BitmapPayload words(kBitmapWords, UINT64_C(0));
    for (const std::uint16_t low : values) {
      const std::size_t word_index =
          static_cast<std::size_t>(low) / 64U;
      const std::size_t bit_index =
          static_cast<std::size_t>(low) % 64U;
      words[word_index] |= UINT64_C(1) << bit_index;
    }
    container.payload = std::move(words);
  }

  static void convert_to_array(Container& container) {
    if (container.is_array()) {
      throw std::logic_error(
          "Roaring-style array conversion requires bitmap payload");
    }

    const auto& words =
        std::get<BitmapPayload>(container.payload);
    ArrayPayload values;
    values.reserve(container.cardinality);

    for (std::size_t word_index = 0U;
         word_index < words.size(); ++word_index) {
      std::uint64_t remaining = words[word_index];
      while (remaining != 0U) {
        const unsigned bit =
            static_cast<unsigned>(std::countr_zero(remaining));
        values.push_back(static_cast<std::uint16_t>(
            word_index * 64U + bit));
        remaining &= remaining - UINT64_C(1);
      }
    }

    if (values.size() != container.cardinality) {
      throw std::logic_error(
          "Roaring-style bitmap cardinality mismatch");
    }
    container.payload = std::move(values);
  }

  [[nodiscard]] static std::size_t rank_low(
      const Container& container,
      const std::uint16_t low) noexcept {
    if (container.is_array()) {
      const auto& values =
          std::get<ArrayPayload>(container.payload);
      return static_cast<std::size_t>(
          std::upper_bound(values.begin(), values.end(), low) -
          values.begin());
    }

    const auto& words =
        std::get<BitmapPayload>(container.payload);
    const std::size_t word_index = static_cast<std::size_t>(low) / 64U;
    const std::size_t bit_index = static_cast<std::size_t>(low) % 64U;

    std::size_t total = 0U;
    for (std::size_t index = 0U; index < word_index; ++index) {
      total += static_cast<std::size_t>(std::popcount(words[index]));
    }

    const std::uint64_t mask =
        bit_index == 63U
            ? std::numeric_limits<std::uint64_t>::max()
            : (UINT64_C(1) << (bit_index + 1U)) - UINT64_C(1);
    total += static_cast<std::size_t>(
        std::popcount(words[word_index] & mask));
    return total;
  }

  [[nodiscard]] static std::uint16_t select_low(
      const Container& container,
      std::size_t index) noexcept {
    if (container.is_array()) {
      return std::get<ArrayPayload>(container.payload)[index];
    }

    const auto& words =
        std::get<BitmapPayload>(container.payload);
    for (std::size_t word_index = 0U;
         word_index < words.size(); ++word_index) {
      std::uint64_t word = words[word_index];
      const std::size_t count =
          static_cast<std::size_t>(std::popcount(word));
      if (index >= count) {
        index -= count;
        continue;
      }

      while (index != 0U) {
        word &= word - UINT64_C(1);
        --index;
      }
      const unsigned bit =
          static_cast<unsigned>(std::countr_zero(word));
      return static_cast<std::uint16_t>(
          word_index * 64U + bit);
    }

    return 0U;
  }
};

}  // namespace algorithms::data_structures
