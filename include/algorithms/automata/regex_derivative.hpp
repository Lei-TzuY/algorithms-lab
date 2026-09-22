#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

namespace algorithms::automata {

// Immutable byte-oriented regular expression with exact Brzozowski derivatives.
//
// The representation deliberately exposes constructors rather than a textual
// parser so syntax/escaping policy stays outside this slice. Matching is
// full-string language membership over arbitrary bytes.
class ByteRegex {
 public:
  enum class Kind : unsigned char {
    empty_set,
    epsilon,
    literal,
    alternate,
    concatenate,
    star,
  };

  [[nodiscard]] static ByteRegex empty() {
    static const auto node = std::make_shared<const Node>(Kind::empty_set);
    return ByteRegex(node);
  }

  [[nodiscard]] static ByteRegex epsilon() {
    static const auto node = std::make_shared<const Node>(Kind::epsilon);
    return ByteRegex(node);
  }

  [[nodiscard]] static ByteRegex literal(const std::uint8_t byte) {
    return ByteRegex(std::make_shared<const Node>(byte));
  }

  [[nodiscard]] static ByteRegex alternate(ByteRegex left, ByteRegex right) {
    if (left.kind() == Kind::empty_set) {
      return right;
    }
    if (right.kind() == Kind::empty_set) {
      return left;
    }
    if (structurally_equal(left.node_, right.node_)) {
      return left;
    }
    return ByteRegex(std::make_shared<const Node>(
        Kind::alternate, std::move(left.node_), std::move(right.node_)));
  }

  [[nodiscard]] static ByteRegex concatenate(ByteRegex left, ByteRegex right) {
    if (left.kind() == Kind::empty_set || right.kind() == Kind::empty_set) {
      return empty();
    }
    if (left.kind() == Kind::epsilon) {
      return right;
    }
    if (right.kind() == Kind::epsilon) {
      return left;
    }
    return ByteRegex(std::make_shared<const Node>(
        Kind::concatenate, std::move(left.node_), std::move(right.node_)));
  }

  [[nodiscard]] static ByteRegex star(ByteRegex operand) {
    if (operand.kind() == Kind::empty_set ||
        operand.kind() == Kind::epsilon) {
      return epsilon();
    }
    if (operand.kind() == Kind::star) {
      return operand;
    }
    return ByteRegex(std::make_shared<const Node>(
        Kind::star, std::move(operand.node_), nullptr));
  }

  [[nodiscard]] Kind kind() const noexcept {
    return node_->kind;
  }

  [[nodiscard]] bool nullable() const {
    return nullable_node(node_);
  }

  // Return the exact left quotient D_byte(L): all suffixes w for which
  // byte followed by w belongs to this regex language.
  [[nodiscard]] ByteRegex derivative(const std::uint8_t byte) const {
    return ByteRegex(derive_node(node_, byte));
  }

  // Full-string language membership.
  [[nodiscard]] bool matches(const std::string_view input) const {
    ByteRegex current = *this;
    for (const char raw : input) {
      const auto byte =
          static_cast<std::uint8_t>(static_cast<unsigned char>(raw));
      current = current.derivative(byte);
      if (current.kind() == Kind::empty_set) {
        return false;
      }
    }
    return current.nullable();
  }

  [[nodiscard]] std::size_t node_count() const noexcept {
    return node_count(node_);
  }

 private:
  struct Node {
    Kind kind;
    std::uint8_t byte{};
    std::shared_ptr<const Node> left;
    std::shared_ptr<const Node> right;

    explicit Node(const Kind node_kind) : kind(node_kind) {}

    explicit Node(const std::uint8_t literal_byte)
        : kind(Kind::literal), byte(literal_byte) {}

    Node(const Kind node_kind,
         std::shared_ptr<const Node> lhs,
         std::shared_ptr<const Node> rhs)
        : kind(node_kind),
          left(std::move(lhs)),
          right(std::move(rhs)) {}
  };

  std::shared_ptr<const Node> node_;

  explicit ByteRegex(std::shared_ptr<const Node> node)
      : node_(std::move(node)) {}

  [[nodiscard]] static bool structurally_equal(
      const std::shared_ptr<const Node>& left,
      const std::shared_ptr<const Node>& right) {
    if (left == right) {
      return true;
    }
    if (left->kind != right->kind || left->byte != right->byte) {
      return false;
    }
    switch (left->kind) {
      case Kind::empty_set:
      case Kind::epsilon:
      case Kind::literal:
        return true;
      case Kind::star:
        return structurally_equal(left->left, right->left);
      case Kind::alternate:
      case Kind::concatenate:
        return structurally_equal(left->left, right->left) &&
               structurally_equal(left->right, right->right);
    }
    return false;
  }

  [[nodiscard]] static bool nullable_node(
      const std::shared_ptr<const Node>& node) {
    switch (node->kind) {
      case Kind::empty_set:
      case Kind::literal:
        return false;
      case Kind::epsilon:
      case Kind::star:
        return true;
      case Kind::alternate:
        return nullable_node(node->left) || nullable_node(node->right);
      case Kind::concatenate:
        return nullable_node(node->left) && nullable_node(node->right);
    }
    return false;
  }

  [[nodiscard]] static std::shared_ptr<const Node> derive_node(
      const std::shared_ptr<const Node>& node,
      const std::uint8_t byte) {
    switch (node->kind) {
      case Kind::empty_set:
      case Kind::epsilon:
        return empty().node_;

      case Kind::literal:
        return node->byte == byte ? epsilon().node_ : empty().node_;

      case Kind::alternate:
        return alternate(
                   ByteRegex(derive_node(node->left, byte)),
                   ByteRegex(derive_node(node->right, byte)))
            .node_;

      case Kind::concatenate: {
        const ByteRegex first = concatenate(
            ByteRegex(derive_node(node->left, byte)),
            ByteRegex(node->right));
        if (!nullable_node(node->left)) {
          return first.node_;
        }
        return alternate(
                   first,
                   ByteRegex(derive_node(node->right, byte)))
            .node_;
      }

      case Kind::star:
        return concatenate(
                   ByteRegex(derive_node(node->left, byte)),
                   ByteRegex(node))
            .node_;
    }
    return empty().node_;
  }

  [[nodiscard]] static std::size_t node_count(
      const std::shared_ptr<const Node>& node) noexcept {
    switch (node->kind) {
      case Kind::empty_set:
      case Kind::epsilon:
      case Kind::literal:
        return 1U;
      case Kind::star:
        return 1U + node_count(node->left);
      case Kind::alternate:
      case Kind::concatenate:
        return 1U + node_count(node->left) + node_count(node->right);
    }
    return 0U;
  }
};

}  // namespace algorithms::automata
