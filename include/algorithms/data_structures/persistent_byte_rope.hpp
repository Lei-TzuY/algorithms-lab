#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace algorithms::data_structures {

namespace detail {
struct PersistentByteRopeNode;
}

class PersistentByteRope {
 public:
  static constexpr std::size_t kMaxLeafBytes = 64;

  PersistentByteRope() noexcept = default;
  explicit PersistentByteRope(std::string_view bytes);

  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t height() const noexcept;
  [[nodiscard]] std::size_t leaf_count() const noexcept;
  [[nodiscard]] std::uint8_t at(std::size_t index) const;
  [[nodiscard]] std::string to_string() const;

  [[nodiscard]] static PersistentByteRope concat(
      const PersistentByteRope& first, const PersistentByteRope& second);
  [[nodiscard]] std::pair<PersistentByteRope, PersistentByteRope> split(
      std::size_t index) const;
  [[nodiscard]] PersistentByteRope insert(std::size_t index,
                                          std::string_view bytes) const;
  [[nodiscard]] PersistentByteRope erase(std::size_t begin,
                                         std::size_t end) const;
  [[nodiscard]] PersistentByteRope slice(std::size_t begin,
                                         std::size_t end) const;

  [[nodiscard]] bool valid_structure() const noexcept;
  [[nodiscard]] std::size_t unique_node_count() const;
  [[nodiscard]] std::size_t shared_node_count_with(
      const PersistentByteRope& other) const;

 private:
  using NodePtr = std::shared_ptr<const detail::PersistentByteRopeNode>;
  explicit PersistentByteRope(NodePtr root);
  NodePtr root_;
};

}  // namespace algorithms::data_structures
