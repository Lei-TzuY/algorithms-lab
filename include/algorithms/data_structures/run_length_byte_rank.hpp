#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace algorithms::data_structures {

class RunLengthByteRankIndex {
 public:
  explicit RunLengthByteRankIndex(std::span<const std::uint8_t> values);

  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] std::size_t run_count() const noexcept;
  [[nodiscard]] std::uint8_t access(std::size_t index) const;
  [[nodiscard]] std::size_t rank(std::uint8_t value, std::size_t end) const;
  [[nodiscard]] std::size_t rank(std::uint8_t value, std::size_t begin,
                                 std::size_t end) const;
  [[nodiscard]] std::size_t logical_payload_bytes() const noexcept;

 private:
  void validate_range(std::size_t begin, std::size_t end) const;
  [[nodiscard]] std::size_t run_end(std::size_t run_index) const noexcept;

  std::size_t size_ = 0U;
  std::vector<std::size_t> run_starts_;
  std::vector<std::uint8_t> run_values_;
  std::vector<std::size_t> run_cumulative_after_;
  std::array<std::vector<std::size_t>, 256U> symbol_run_indices_{};
  std::size_t logical_payload_bytes_ = 0U;
};

}  // namespace algorithms::data_structures
