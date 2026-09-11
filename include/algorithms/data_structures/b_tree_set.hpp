#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace algorithms::data_structures {

struct BTreeMutationDiagnostics {
  std::size_t splits{};
  std::size_t borrows_from_previous{};
  std::size_t borrows_from_next{};
  std::size_t merges{};
  std::size_t root_shrinks{};
};

class BTreeSet {
 public:
  explicit BTreeSet(std::size_t minimum_degree);
  ~BTreeSet();

  BTreeSet(const BTreeSet&) = delete;
  BTreeSet& operator=(const BTreeSet&) = delete;
  BTreeSet(BTreeSet&&) = delete;
  BTreeSet& operator=(BTreeSet&&) = delete;

  [[nodiscard]] std::size_t minimum_degree() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] std::size_t height() const noexcept;

  [[nodiscard]] bool contains(std::int64_t key) const;
  bool insert(std::int64_t key);
  bool erase(std::int64_t key);

  [[nodiscard]] std::vector<std::int64_t> values_in_order() const;
  [[nodiscard]] bool valid_structure() const;
  [[nodiscard]] const BTreeMutationDiagnostics& diagnostics() const noexcept;

 private:
  struct Node;

  [[nodiscard]] std::size_t max_keys() const noexcept;
  [[nodiscard]] std::size_t lower_bound_index(const Node& node,
                                              std::int64_t key) const noexcept;
  [[nodiscard]] bool contains(const Node& node, std::int64_t key) const;

  void split_child(Node& parent, std::size_t child_index);
  void insert_non_full(Node& node, std::int64_t key);

  void erase_existing(Node& node, std::int64_t key);
  void erase_from_internal(Node& node, std::size_t key_index);
  void ensure_child_can_lose_key(Node& parent, std::size_t& child_index);
  void borrow_from_previous(Node& parent, std::size_t child_index);
  void borrow_from_next(Node& parent, std::size_t child_index);
  void merge_children(Node& parent, std::size_t key_index);

  [[nodiscard]] std::int64_t subtree_minimum(const Node& node) const;
  [[nodiscard]] std::int64_t subtree_maximum(const Node& node) const;
  void collect_in_order(const Node& node, std::vector<std::int64_t>& out) const;

  std::size_t minimum_degree_{};
  std::size_t size_{};
  std::unique_ptr<Node> root_;
  BTreeMutationDiagnostics diagnostics_{};
};

}  // namespace algorithms::data_structures
