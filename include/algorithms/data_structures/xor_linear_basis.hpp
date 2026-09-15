#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace algorithms::data_structures {

class XorLinearBasis64 {
 public:
  [[nodiscard]] std::size_t rank() const noexcept { return rank_; }
  [[nodiscard]] bool empty() const noexcept { return rank_ == 0; }

  bool insert(std::uint64_t value) noexcept {
    std::uint64_t reduced = value;

    for (std::size_t bit = 64; bit-- > 0;) {
      const std::uint64_t mask = std::uint64_t{1} << bit;
      if ((reduced & mask) == 0) {
        continue;
      }
      if (basis_[bit] != 0) {
        reduced ^= basis_[bit];
        continue;
      }

      for (std::size_t lower = bit; lower-- > 0;) {
        if (basis_[lower] != 0 &&
            (reduced & (std::uint64_t{1} << lower)) != 0) {
          reduced ^= basis_[lower];
        }
      }

      basis_[bit] = reduced;
      ++rank_;

      for (std::size_t higher = bit + 1; higher < 64; ++higher) {
        if (basis_[higher] != 0 && (basis_[higher] & mask) != 0) {
          basis_[higher] ^= reduced;
        }
      }
      return true;
    }

    return false;
  }

  [[nodiscard]] bool contains(std::uint64_t value) const noexcept {
    return reduce(value) == 0;
  }

  [[nodiscard]] std::uint64_t maximize_xor(
      std::uint64_t seed = 0) const noexcept {
    std::uint64_t result = seed;
    for (std::size_t bit = 64; bit-- > 0;) {
      if (basis_[bit] == 0) {
        continue;
      }
      const std::uint64_t candidate = result ^ basis_[bit];
      if (candidate > result) {
        result = candidate;
      }
    }
    return result;
  }

  [[nodiscard]] std::vector<std::uint64_t> canonical_basis() const {
    std::vector<std::uint64_t> rows;
    rows.reserve(rank_);
    for (std::size_t bit = 64; bit-- > 0;) {
      if (basis_[bit] != 0) {
        rows.push_back(basis_[bit]);
      }
    }
    return rows;
  }

  [[nodiscard]] bool valid_structure() const noexcept {
    std::size_t counted = 0;
    for (std::size_t bit = 0; bit < 64; ++bit) {
      if (basis_[bit] == 0) {
        continue;
      }
      ++counted;
      if ((basis_[bit] & (std::uint64_t{1} << bit)) == 0) {
        return false;
      }
      if ((basis_[bit] >> bit) != 1) {
        return false;
      }
      for (std::size_t other = 0; other < 64; ++other) {
        if (other != bit && basis_[other] != 0 &&
            (basis_[bit] & (std::uint64_t{1} << other)) != 0) {
          return false;
        }
      }
    }
    return counted == rank_;
  }

 private:
  [[nodiscard]] std::uint64_t reduce(std::uint64_t value) const noexcept {
    std::uint64_t reduced = value;
    for (std::size_t bit = 64; bit-- > 0;) {
      const std::uint64_t mask = std::uint64_t{1} << bit;
      if ((reduced & mask) != 0 && basis_[bit] != 0) {
        reduced ^= basis_[bit];
      }
    }
    return reduced;
  }

  std::array<std::uint64_t, 64> basis_{};
  std::size_t rank_ = 0;
};

}  // namespace algorithms::data_structures
