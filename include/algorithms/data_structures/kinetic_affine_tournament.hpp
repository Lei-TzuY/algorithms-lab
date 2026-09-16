#pragma once

#include "algorithms/data_structures/binary_heap.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

struct AffineTrajectory {
  std::int64_t slope = 0;
  std::int64_t intercept = 0;
  friend bool operator==(const AffineTrajectory&, const AffineTrajectory&) = default;
};

struct KineticMinimumWitness {
  std::size_t trajectory = 0;
  std::int64_t value = 0;
  friend bool operator==(const KineticMinimumWitness&, const KineticMinimumWitness&) = default;
};

class DiscreteKineticAffineTournament {
 public:
  static constexpr std::int64_t coordinate_bound = 1'000'000'000LL;
  static constexpr std::int64_t time_bound = 1'000'000'000LL;

  explicit DiscreteKineticAffineTournament(std::span<const AffineTrajectory> trajectories,
                                   std::int64_t initial_time = -time_bound)
      : trajectories_(trajectories.begin(), trajectories.end()), current_time_(initial_time) {
    validate_time(initial_time);
    for (const auto& trajectory : trajectories_) {
      validate_coefficient(trajectory.slope);
      validate_coefficient(trajectory.intercept);
    }
    if (trajectories_.size() > (std::numeric_limits<std::size_t>::max() / 2U)) {
      throw std::length_error("kinetic tournament too large");
    }
    leaf_base_ = 1;
    while (leaf_base_ < trajectories_.size()) {
      if (leaf_base_ > (std::numeric_limits<std::size_t>::max() / 2U)) {
        throw std::length_error("kinetic tournament tree size overflow");
      }
      leaf_base_ *= 2;
    }
    winners_.assign(leaf_base_ * 2, no_vertex());
    generations_.assign(leaf_base_, 0);
    scheduled_.assign(leaf_base_, std::nullopt);
    for (std::size_t index = 0; index < trajectories_.size(); ++index) {
      winners_[leaf_base_ + index] = index;
    }
    if (leaf_base_ > 1) {
      for (std::size_t node = leaf_base_ - 1; node > 0; --node) {
        winners_[node] = preferred(winners_[node * 2], winners_[node * 2 + 1]);
      }
      for (std::size_t node = 1; node < leaf_base_; ++node) {
        reschedule(node);
      }
    }
  }

  [[nodiscard]] std::size_t size() const noexcept { return trajectories_.size(); }
  [[nodiscard]] std::int64_t current_time() const noexcept { return current_time_; }
  [[nodiscard]] std::size_t processed_certificate_failures() const noexcept { return processed_failures_; }

  [[nodiscard]] std::optional<KineticMinimumWitness> minimum() const {
    if (trajectories_.empty()) return std::nullopt;
    const std::size_t winner = winners_[1];
    return KineticMinimumWitness{winner, value_at(winner, current_time_)};
  }

  [[nodiscard]] std::optional<std::int64_t> next_certificate_failure_time() const {
    std::optional<std::int64_t> answer;
    for (std::size_t node = 1; node < leaf_base_; ++node) {
      if (scheduled_[node] && (!answer || *scheduled_[node] < *answer)) answer = scheduled_[node];
    }
    return answer;
  }

  std::size_t advance_to(std::int64_t target_time) {
    validate_time(target_time);
    if (target_time < current_time_) throw std::invalid_argument("kinetic time must be monotone");
    const std::size_t before = processed_failures_;
    while (!events_.empty()) {
      const Event event = events_.top();
      if (event.time > target_time) break;
      static_cast<void>(events_.pop());
      if (event.node >= leaf_base_) continue;
      if (event.generation != generations_[event.node]) continue;
      if (!scheduled_[event.node] || *scheduled_[event.node] != event.time) continue;
      if (event.time < current_time_) continue;
      current_time_ = event.time;
      ++processed_failures_;
      recompute_to_root(event.node);
      maybe_compact_events();
    }
    current_time_ = target_time;
    maybe_compact_events();
    return processed_failures_ - before;
  }

  [[nodiscard]] bool valid_structure() const {
    if (winners_.size() != leaf_base_ * 2 || generations_.size() != leaf_base_ ||
        scheduled_.size() != leaf_base_ || !events_.valid_heap_property()) return false;
    for (std::size_t index = 0; index < leaf_base_; ++index) {
      const auto expected = index < trajectories_.size() ? index : no_vertex();
      if (winners_[leaf_base_ + index] != expected) return false;
    }
    if (leaf_base_ > 1) {
      for (std::size_t node = leaf_base_ - 1; node > 0; --node) {
        const std::size_t expected = preferred(winners_[node * 2], winners_[node * 2 + 1]);
        if (winners_[node] != expected) return false;
        if (scheduled_[node] != certificate_failure(node)) return false;
      }
    }
    if (trajectories_.empty()) return winners_[1] == no_vertex();
    std::size_t scan = 0;
    for (std::size_t index = 1; index < trajectories_.size(); ++index) {
      if (better(index, scan, current_time_)) scan = index;
    }
    return winners_[1] == scan;
  }

 private:
  struct Event {
    std::int64_t time = 0;
    std::size_t node = 0;
    std::size_t generation = 0;
  };
  struct EventCompare {
    bool operator()(const Event& a, const Event& b) const noexcept {
      if (a.time != b.time) return a.time < b.time;
      return a.node > b.node;  // descendants before ancestors for simultaneous failures
    }
  };

  static constexpr std::size_t no_vertex() noexcept { return std::numeric_limits<std::size_t>::max(); }
  static void validate_coefficient(std::int64_t value) {
    if (value < -coordinate_bound || value > coordinate_bound) throw std::out_of_range("kinetic affine coefficient outside exact domain");
  }
  static void validate_time(std::int64_t value) {
    if (value < -time_bound || value > time_bound) throw std::out_of_range("kinetic time outside exact domain");
  }
  [[nodiscard]] std::int64_t value_at(std::size_t id, std::int64_t time) const {
    return trajectories_[id].slope * time + trajectories_[id].intercept;
  }
  [[nodiscard]] bool better(std::size_t first, std::size_t second, std::int64_t time) const {
    if (first == no_vertex()) return false;
    if (second == no_vertex()) return true;
    const auto a = value_at(first, time);
    const auto b = value_at(second, time);
    return a < b || (a == b && first < second);
  }
  [[nodiscard]] std::size_t preferred(std::size_t left, std::size_t right) const {
    return better(left, right, current_time_) ? left : right;
  }
  static std::int64_t ceil_div(std::int64_t numerator, std::int64_t denominator) {
    const auto quotient = numerator / denominator;
    const auto remainder = numerator % denominator;
    return quotient + ((remainder != 0 && numerator > 0) ? 1 : 0);
  }
  [[nodiscard]] std::optional<std::int64_t> loser_failure(std::size_t winner, std::size_t loser) const {
    if (winner == no_vertex() || loser == no_vertex()) return std::nullopt;
    const std::int64_t delta_slope = trajectories_[loser].slope - trajectories_[winner].slope;
    if (delta_slope >= 0 || current_time_ == time_bound) return std::nullopt;
    const std::int64_t delta_intercept = trajectories_[loser].intercept - trajectories_[winner].intercept;
    const std::int64_t threshold = loser < winner ? 0 : -1;
    const std::int64_t denominator = -delta_slope;
    const std::int64_t numerator = delta_intercept - threshold;
    std::int64_t candidate = ceil_div(numerator, denominator);
    if (candidate <= current_time_) candidate = current_time_ + 1;
    if (candidate > time_bound) return std::nullopt;
    return candidate;
  }
  [[nodiscard]] std::optional<std::int64_t> certificate_failure(std::size_t node) const {
    const auto left = winners_[node * 2];
    const auto right = winners_[node * 2 + 1];
    if (left == no_vertex() || right == no_vertex()) return std::nullopt;
    const auto winner = better(left, right, current_time_) ? left : right;
    const auto loser = winner == left ? right : left;
    return loser_failure(winner, loser);
  }
  void reschedule(std::size_t node) {
    ++generations_[node];
    scheduled_[node] = certificate_failure(node);
    if (scheduled_[node]) events_.push(Event{*scheduled_[node], node, generations_[node]});
  }
  void recompute_to_root(std::size_t node) {
    while (node > 0) {
      winners_[node] = preferred(winners_[node * 2], winners_[node * 2 + 1]);
      reschedule(node);
      node /= 2;
    }
  }
  void maybe_compact_events() {
    const std::size_t threshold = leaf_base_ > (std::numeric_limits<std::size_t>::max() - 16U) / 8U
                                      ? std::numeric_limits<std::size_t>::max()
                                      : leaf_base_ * 8U + 16U;
    if (events_.size() <= threshold) return;
    BinaryHeap<Event, EventCompare> rebuilt;
    for (std::size_t node = 1; node < leaf_base_; ++node) {
      if (scheduled_[node]) rebuilt.push(Event{*scheduled_[node], node, generations_[node]});
    }
    events_ = std::move(rebuilt);
  }

  std::vector<AffineTrajectory> trajectories_;
  std::int64_t current_time_ = 0;
  std::size_t leaf_base_ = 1;
  std::vector<std::size_t> winners_;
  std::vector<std::size_t> generations_;
  std::vector<std::optional<std::int64_t>> scheduled_;
  BinaryHeap<Event, EventCompare> events_;
  std::size_t processed_failures_ = 0;
};

}  // namespace algorithms::data_structures
