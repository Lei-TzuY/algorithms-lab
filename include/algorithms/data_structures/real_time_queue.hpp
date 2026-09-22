#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <variant>

namespace algorithms::data_structures {

// Persistent Hood-Melville real-time FIFO queue.
//
// Each update returns a new queue version and leaves the source version
// unchanged. A rear-list reversal is represented as an explicit rotation state;
// every push/pop executes at most two constant-size rotation transitions.
class RealTimeQueue {
 public:
  using Value = std::int64_t;

  RealTimeQueue() = default;

  [[nodiscard]] bool empty() const noexcept { return size() == 0U; }

  [[nodiscard]] std::size_t size() const noexcept {
    return front_size_ + rear_size_;
  }

  [[nodiscard]] Value front() const {
    if (empty() || !front_) {
      throw std::out_of_range("front of empty real-time queue");
    }
    return front_->value;
  }

  [[nodiscard]] std::size_t last_rotation_work() const noexcept {
    return last_rotation_work_;
  }

  [[nodiscard]] bool rotation_active() const noexcept {
    return !std::holds_alternative<Idle>(rotation_);
  }

  [[nodiscard]] RealTimeQueue push_back(const Value value) const {
    if (size() == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error("real-time queue size exhausted");
    }

    RealTimeQueue next = *this;
    next.last_rotation_work_ = 0U;
    next.rear_ = cons(value, next.rear_);
    ++next.rear_size_;
    next.check();
    return next;
  }

  [[nodiscard]] RealTimeQueue pop_front() const {
    if (empty() || !front_) {
      throw std::out_of_range("pop_front on empty real-time queue");
    }

    RealTimeQueue next = *this;
    next.last_rotation_work_ = 0U;
    --next.front_size_;
    next.front_ = next.front_->next;
    next.rotation_ = next.invalidate(next.rotation_);
    next.check();
    return next;
  }

  // Structural diagnostic. Semantic FIFO/persistence correctness is verified
  // independently by tests against branching std::deque histories.
  [[nodiscard]] bool valid_invariants() const noexcept {
    if (rear_size_ > front_size_) {
      return false;
    }
    if (size() == 0U) {
      if (front_ || rear_ || front_size_ != 0U || rear_size_ != 0U ||
          !std::holds_alternative<Idle>(rotation_)) {
        return false;
      }
      return true;
    }
    if (!front_) {
      return false;
    }
    if (last_rotation_work_ > 2U) {
      return false;
    }

    const std::size_t rear_nodes =
        bounded_list_length(rear_, rear_size_ + 1U);
    if (rear_nodes != rear_size_) {
      return false;
    }

    if (std::holds_alternative<Idle>(rotation_)) {
      const std::size_t front_nodes =
          bounded_list_length(front_, front_size_ + 1U);
      return front_nodes == front_size_;
    }

    if (const auto* reversing = std::get_if<Reversing>(&rotation_)) {
      if (reversing->ok < 0) {
        return false;
      }
      const std::size_t f =
          bounded_list_length(reversing->front, front_size_ + 1U);
      const std::size_t r =
          bounded_list_length(reversing->rear, front_size_ + 2U);
      if (f == kTooLong || r == kTooLong || r != f + 1U) {
        return false;
      }
      return true;
    }

    if (const auto* appending = std::get_if<Appending>(&rotation_)) {
      if (appending->ok < 0) {
        return false;
      }
      const std::size_t copied =
          bounded_list_length(appending->front_reversed, front_size_ + 1U);
      const std::size_t candidate =
          bounded_list_length(appending->rear_reversed, front_size_ + 2U);
      if (copied == kTooLong || candidate == kTooLong ||
          static_cast<std::size_t>(appending->ok) > copied ||
          candidate == 0U) {
        return false;
      }
      return true;
    }

    // Done is installed into front_ before any public operation returns.
    return false;
  }

 private:
  struct Node;
  using List = std::shared_ptr<const Node>;

  struct Node {
    Value value;
    List next;

    Node(const Value node_value, List node_next)
        : value(node_value), next(std::move(node_next)) {}
  };

  struct Idle {};

  struct Reversing {
    std::ptrdiff_t ok;
    List front;
    List front_reversed;
    List rear;
    List rear_reversed;
  };

  struct Appending {
    std::ptrdiff_t ok;
    List front_reversed;
    List rear_reversed;
  };

  struct Done {
    List front;
  };

  using Rotation = std::variant<Idle, Reversing, Appending, Done>;

  static constexpr std::size_t kTooLong =
      std::numeric_limits<std::size_t>::max();

  std::size_t front_size_{};
  List front_;
  Rotation rotation_{Idle{}};
  std::size_t rear_size_{};
  List rear_;
  std::size_t last_rotation_work_{};

  [[nodiscard]] static List cons(const Value value, const List& tail) {
    return std::make_shared<const Node>(value, tail);
  }

  [[nodiscard]] static std::size_t bounded_list_length(
      List list, const std::size_t limit) noexcept {
    std::size_t length = 0U;
    while (list) {
      if (length >= limit) {
        return kTooLong;
      }
      ++length;
      list = list->next;
    }
    return length;
  }

  [[nodiscard]] Rotation execute_once(const Rotation& state) {
    if (const auto* reversing = std::get_if<Reversing>(&state)) {
      if (reversing->front && reversing->rear) {
        ++last_rotation_work_;
        return Reversing{
            reversing->ok + 1,
            reversing->front->next,
            cons(reversing->front->value, reversing->front_reversed),
            reversing->rear->next,
            cons(reversing->rear->value, reversing->rear_reversed)};
      }

      if (!reversing->front && reversing->rear &&
          !reversing->rear->next) {
        ++last_rotation_work_;
        return Appending{
            reversing->ok,
            reversing->front_reversed,
            cons(reversing->rear->value, reversing->rear_reversed)};
      }

      throw std::logic_error("invalid real-time queue reversing state");
    }

    if (const auto* appending = std::get_if<Appending>(&state)) {
      if (appending->ok == 0) {
        ++last_rotation_work_;
        return Done{appending->rear_reversed};
      }
      if (appending->ok > 0 && appending->front_reversed) {
        ++last_rotation_work_;
        return Appending{
            appending->ok - 1,
            appending->front_reversed->next,
            cons(appending->front_reversed->value,
                 appending->rear_reversed)};
      }
      throw std::logic_error("invalid real-time queue appending state");
    }

    return state;
  }

  [[nodiscard]] Rotation invalidate(const Rotation& state) const {
    if (const auto* reversing = std::get_if<Reversing>(&state)) {
      if (reversing->ok <= 0) {
        throw std::logic_error(
            "real-time queue invalidated unfinished rotation");
      }
      Reversing updated = *reversing;
      --updated.ok;
      return updated;
    }

    if (const auto* appending = std::get_if<Appending>(&state)) {
      if (appending->ok == 0) {
        if (!appending->rear_reversed) {
          throw std::logic_error(
              "real-time queue appending candidate exhausted");
        }
        return Done{appending->rear_reversed->next};
      }
      if (appending->ok > 0) {
        Appending updated = *appending;
        --updated.ok;
        return updated;
      }
      throw std::logic_error("invalid real-time queue append counter");
    }

    return state;
  }

  void execute_twice() {
    rotation_ = execute_once(rotation_);
    rotation_ = execute_once(rotation_);

    if (const auto* done = std::get_if<Done>(&rotation_)) {
      front_ = done->front;
      rotation_ = Idle{};
    }
  }

  void check() {
    if (rear_size_ <= front_size_) {
      execute_twice();
      return;
    }

    front_size_ += rear_size_;
    rear_size_ = 0U;
    rotation_ = Reversing{0, front_, nullptr, rear_, nullptr};
    rear_.reset();
    execute_twice();
  }
};

}  // namespace algorithms::data_structures
