#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace algorithms::streaming {
struct CountMinHashRow {
  std::uint64_t multiplier{};
  std::uint64_t increment{};
  friend bool operator==(const CountMinHashRow&, const CountMinHashRow&) = default;
};
class CountMinSketch {
 public:
  static constexpr std::uint64_t hash_prime() noexcept { return 4'294'967'311ULL; }
  CountMinSketch(std::size_t width, std::vector<CountMinHashRow> rows);
  void add(std::uint32_t item, std::uint64_t amount = 1U);
  [[nodiscard]] std::uint64_t estimate(std::uint32_t item) const;
  [[nodiscard]] std::size_t width() const noexcept { return width_; }
  [[nodiscard]] std::size_t depth() const noexcept { return rows_.size(); }
  [[nodiscard]] std::uint64_t total_weight() const noexcept { return total_weight_; }
  [[nodiscard]] const std::vector<CountMinHashRow>& hash_rows() const noexcept { return rows_; }
  [[nodiscard]] bool valid_state() const noexcept;
 private:
  [[nodiscard]] std::size_t bucket(std::size_t row_index, std::uint32_t item) const;
  std::size_t width_{};
  std::vector<CountMinHashRow> rows_;
  std::vector<std::uint64_t> counters_;
  std::uint64_t total_weight_{};
};
}
