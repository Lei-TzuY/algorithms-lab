#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace algorithms::strings {

struct RollingFingerprint {
  std::uint64_t first{};
  std::uint64_t second{};

  bool operator==(const RollingFingerprint&) const = default;
};

// Deterministic, non-cryptographic substring fingerprints.
// Fingerprint equality is a collision-prone candidate filter, not proof of
// string equality.
class RollingHash {
 public:
  static constexpr std::uint64_t kBase = 257ULL;
  static constexpr std::uint64_t kModulus1 = 1'000'000'007ULL;
  static constexpr std::uint64_t kModulus2 = 1'000'000'009ULL;

  explicit RollingHash(std::string_view input) : size_(input.size()) {
    if (size_ == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error("rolling hash input is too large");
    }

    prefix1_.assign(size_ + 1U, 0ULL);
    prefix2_.assign(size_ + 1U, 0ULL);
    powers1_.assign(size_ + 1U, 1ULL);
    powers2_.assign(size_ + 1U, 1ULL);

    for (std::size_t index = 0U; index < size_; ++index) {
      const std::uint64_t symbol =
          static_cast<std::uint64_t>(static_cast<unsigned char>(input[index])) +
          std::uint64_t{1};
      prefix1_[index + 1U] =
          (prefix1_[index] * kBase + symbol) % kModulus1;
      prefix2_[index + 1U] =
          (prefix2_[index] * kBase + symbol) % kModulus2;
      powers1_[index + 1U] = (powers1_[index] * kBase) % kModulus1;
      powers2_[index + 1U] = (powers2_[index] * kBase) % kModulus2;
    }
  }

  [[nodiscard]] std::size_t size() const noexcept { return size_; }

  [[nodiscard]] RollingFingerprint fingerprint(std::size_t begin,
                                               std::size_t end) const {
    if (begin > end || end > size_) {
      throw std::out_of_range("rolling hash range is outside the snapshot");
    }

    const std::size_t length = end - begin;
    return RollingFingerprint{
        subtract_prefix(prefix1_[end], prefix1_[begin], powers1_[length],
                        kModulus1),
        subtract_prefix(prefix2_[end], prefix2_[begin], powers2_[length],
                        kModulus2)};
  }

 private:
  [[nodiscard]] static std::uint64_t subtract_prefix(
      std::uint64_t whole, std::uint64_t prefix, std::uint64_t power,
      std::uint64_t modulus) noexcept {
    const std::uint64_t removed = (prefix * power) % modulus;
    return (whole + modulus - removed) % modulus;
  }

  std::size_t size_{};
  std::vector<std::uint64_t> prefix1_;
  std::vector<std::uint64_t> prefix2_;
  std::vector<std::uint64_t> powers1_;
  std::vector<std::uint64_t> powers2_;
};

}  // namespace algorithms::strings
