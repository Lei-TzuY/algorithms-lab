#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace algorithms::data_structures {

class VanEmdeBoasSet {
 public:
  explicit VanEmdeBoasSet(unsigned universe_bits);
  ~VanEmdeBoasSet();

  VanEmdeBoasSet(const VanEmdeBoasSet&) = delete;
  VanEmdeBoasSet& operator=(const VanEmdeBoasSet&) = delete;
  VanEmdeBoasSet(VanEmdeBoasSet&&) = delete;
  VanEmdeBoasSet& operator=(VanEmdeBoasSet&&) = delete;

  [[nodiscard]] std::uint32_t universe_size() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool empty() const noexcept;

  bool insert(std::uint32_t key);
  bool erase(std::uint32_t key);
  [[nodiscard]] bool contains(std::uint32_t key) const;

  [[nodiscard]] std::optional<std::uint32_t> minimum() const noexcept;
  [[nodiscard]] std::optional<std::uint32_t> maximum() const noexcept;
  [[nodiscard]] std::optional<std::uint32_t> predecessor(std::uint32_t key) const;
  [[nodiscard]] std::optional<std::uint32_t> successor(std::uint32_t key) const;

 private:
  struct Node;

  void validate_key(std::uint32_t key) const;

  std::unique_ptr<Node> root_;
  std::uint32_t universe_size_{};
  std::size_t size_{};
};

}  // namespace algorithms::data_structures
