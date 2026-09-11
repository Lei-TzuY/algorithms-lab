#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>

namespace algorithms::data_structures {

class FibonacciMinHeap {
 public:
  using Key = std::int64_t;
  using Handle = std::uint64_t;

  struct Entry {
    Handle handle{};
    Key key{};

    friend bool operator==(const Entry&, const Entry&) = default;
  };

  FibonacciMinHeap() = default;
  ~FibonacciMinHeap() = default;

  FibonacciMinHeap(const FibonacciMinHeap&) = delete;
  FibonacciMinHeap& operator=(const FibonacciMinHeap&) = delete;

  FibonacciMinHeap(FibonacciMinHeap&& other) noexcept;
  FibonacciMinHeap& operator=(FibonacciMinHeap&& other) noexcept;

  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool contains(Handle handle) const noexcept;

  [[nodiscard]] Entry minimum() const;
  [[nodiscard]] Handle insert(Key key);
  void decrease_key(Handle handle, Key new_key);
  [[nodiscard]] Entry extract_min();

  // Destructively melds other into this heap. Handles originating in either
  // heap remain valid in *this; other becomes empty.
  void meld(FibonacciMinHeap&& other);

  // Executable structural diagnostics for the educational implementation.
  [[nodiscard]] std::size_t root_count() const noexcept;
  [[nodiscard]] std::size_t marked_count() const noexcept;
  [[nodiscard]] std::size_t potential() const noexcept;
  [[nodiscard]] bool valid_structure() const;

 private:
  struct Node {
    Handle handle{};
    Key key{};
    std::size_t degree{};
    bool mark{};
    Node* parent{};
    Node* child{};
    Node* left{};
    Node* right{};
  };

  static Handle allocate_handle();

  [[nodiscard]] Node* find_node(Handle handle);
  [[nodiscard]] const Node* find_node(Handle handle) const;

  static bool root_better(const Node* first, const Node* second) noexcept;
  static void make_singleton(Node* node) noexcept;
  static void splice_after(Node* position, Node* node) noexcept;

  void add_root(Node* node) noexcept;
  void add_child(Node* child, Node* parent) noexcept;
  void remove_child(Node* child, Node* parent) noexcept;
  void cut(Node* child, Node* parent) noexcept;
  void cascading_cut(Node* node) noexcept;
  void consolidate();

  [[nodiscard]] bool validate_subtree(
      const Node* node, const Node* expected_parent,
      std::unordered_map<Handle, bool>& seen, std::size_t& marked) const;

  std::unordered_map<Handle, std::unique_ptr<Node>> nodes_;
  Node* min_{};
  std::size_t root_count_{};
  std::size_t marked_count_{};
};

}  // namespace algorithms::data_structures
