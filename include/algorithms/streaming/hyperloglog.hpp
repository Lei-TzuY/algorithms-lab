#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace algorithms::streaming {

class HyperLogLog {
 public:
  static constexpr std::uint8_t minimum_precision() noexcept { return 4U; }
  static constexpr std::uint8_t maximum_precision() noexcept { return 20U; }

  HyperLogLog(const std::uint8_t precision, const std::uint64_t seed)
      : precision_(precision), seed_(seed) {
    if (precision < minimum_precision() || precision > maximum_precision()) {
      throw std::invalid_argument("HyperLogLog precision is outside [4, 20]");
    }
    registers_.assign(std::size_t{1} << precision_, std::uint8_t{0});
  }

  void add(const std::uint64_t item) noexcept {
    const std::uint64_t hash = mix(item, seed_);
    const std::size_t index = bucket(hash);
    registers_[index] = std::max(registers_[index], rank(hash));
  }

  void merge(const HyperLogLog& other) {
    if (precision_ != other.precision_ || seed_ != other.seed_) {
      throw std::invalid_argument(
          "HyperLogLog merge requires identical precision and seed");
    }
    for (std::size_t index = 0U; index < registers_.size(); ++index) {
      registers_[index] = std::max(registers_[index], other.registers_[index]);
    }
  }

  [[nodiscard]] double estimate() const noexcept {
    const double count = static_cast<double>(registers_.size());
    double harmonic_sum = 0.0;
    std::size_t zero_count = 0U;
    for (const std::uint8_t value : registers_) {
      harmonic_sum += std::ldexp(1.0, -static_cast<int>(value));
      if (value == 0U) {
        ++zero_count;
      }
    }

    const double raw = alpha() * count * count / harmonic_sum;
    if (raw <= 2.5 * count && zero_count != 0U) {
      return count * std::log(count / static_cast<double>(zero_count));
    }
    return raw;
  }

  [[nodiscard]] std::uint8_t precision() const noexcept { return precision_; }
  [[nodiscard]] std::size_t register_count() const noexcept {
    return registers_.size();
  }
  [[nodiscard]] std::uint64_t seed() const noexcept { return seed_; }
  [[nodiscard]] const std::vector<std::uint8_t>& registers() const noexcept {
    return registers_;
  }

  [[nodiscard]] std::size_t zero_registers() const noexcept {
    return static_cast<std::size_t>(
        std::count(registers_.begin(), registers_.end(), std::uint8_t{0}));
  }

  [[nodiscard]] bool valid_state() const noexcept {
    if (precision_ < minimum_precision() || precision_ > maximum_precision() ||
        registers_.size() != (std::size_t{1} << precision_)) {
      return false;
    }
    const auto maximum_rank =
        static_cast<std::uint8_t>((64U - precision_) + 1U);
    return std::all_of(registers_.begin(), registers_.end(),
                       [maximum_rank](const std::uint8_t value) {
                         return value <= maximum_rank;
                       });
  }

 private:
  [[nodiscard]] static std::uint64_t mix(
      std::uint64_t item, const std::uint64_t seed) noexcept {
    std::uint64_t value = item ^ seed;
    value += UINT64_C(0x9E3779B97F4A7C15);
    value = (value ^ (value >> 30U)) * UINT64_C(0xBF58476D1CE4E5B9);
    value = (value ^ (value >> 27U)) * UINT64_C(0x94D049BB133111EB);
    return value ^ (value >> 31U);
  }

  [[nodiscard]] std::size_t bucket(const std::uint64_t hash) const noexcept {
    return static_cast<std::size_t>(hash >> (64U - precision_));
  }

  [[nodiscard]] std::uint8_t rank(const std::uint64_t hash) const noexcept {
    const std::uint64_t suffix = hash << precision_;
    const unsigned int available_bits = 64U - precision_;
    unsigned int result =
        static_cast<unsigned int>(std::countl_zero(suffix)) + 1U;
    const unsigned int maximum_rank = available_bits + 1U;
    if (result > maximum_rank) {
      result = maximum_rank;
    }
    return static_cast<std::uint8_t>(result);
  }

  [[nodiscard]] double alpha() const noexcept {
    switch (registers_.size()) {
      case 16U:
        return 0.673;
      case 32U:
        return 0.697;
      case 64U:
        return 0.709;
      default: {
        const double count = static_cast<double>(registers_.size());
        return 0.7213 / (1.0 + 1.079 / count);
      }
    }
  }

  std::uint8_t precision_{};
  std::uint64_t seed_{};
  std::vector<std::uint8_t> registers_;
};

}  // namespace algorithms::streaming
