#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace algorithms::streaming {

struct MisraGriesCounter {
  std::int64_t item;
  std::size_t residual_count;

  friend bool operator==(const MisraGriesCounter&, const MisraGriesCounter&) = default;
};

struct MisraGriesSummary {
  std::size_t k;
  std::size_t processed_count;
  std::size_t decrement_rounds;
  std::vector<MisraGriesCounter> counters;
};

class MisraGries {
 public:
  explicit MisraGries(std::size_t k);

  void update(std::int64_t item);

  [[nodiscard]] std::size_t k() const noexcept;
  [[nodiscard]] std::size_t processed_count() const noexcept;
  [[nodiscard]] std::size_t decrement_rounds() const noexcept;
  [[nodiscard]] std::size_t counter_count() const noexcept;
  [[nodiscard]] std::size_t residual_count(std::int64_t item) const noexcept;
  [[nodiscard]] bool is_candidate(std::int64_t item) const noexcept;
  [[nodiscard]] MisraGriesSummary summary() const;

 private:
  std::size_t k_;
  std::size_t processed_count_ = 0;
  std::size_t decrement_rounds_ = 0;
  std::vector<MisraGriesCounter> counters_;
};

[[nodiscard]] MisraGriesSummary misra_gries_summary(
    std::span<const std::int64_t> stream, std::size_t k);

}  // namespace algorithms::streaming
