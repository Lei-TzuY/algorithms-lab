#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace algorithms::data_structures {

class SplaySet {
 public:
  using Key = std::int64_t;

  SplaySet() = default;
  ~SplaySet();

  SplaySet(const SplaySet&) = delete;
  SplaySet& operator=(const SplaySet&) = delete;
  SplaySet(SplaySet&&) = delete;
  SplaySet& operator=(SplaySet&&) = delete;

  [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] std::optional<Key> root_key() const noexcept;

  // Search splays the matching node, or the last visited node on a miss.
  bool contains(Key key);
  bool insert(Key key);
  bool erase(Key key);

  [[nodiscard]] std::vector<Key> inorder_keys() const;
  [[nodiscard]] bool valid_invariants() const;

 private:
  struct Node {
    Key key;
    Node* parent = nullptr;
    Node* left = nullptr;
    Node* right = nullptr;
  };

  Node* root_ = nullptr;
  std::size_t size_ = 0;

  void rotate_left(Node* node) noexcept;
  void rotate_right(Node* node) noexcept;
  void splay(Node* node) noexcept;
  Node* access(Key key) noexcept;
  void clear() noexcept;
};

}  // namespace algorithms::data_structures
