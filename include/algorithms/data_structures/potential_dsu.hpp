#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

class PotentialDisjointSetUnion {
 public:
  enum class ConstraintResult {
    merged,
    already_satisfied,
    contradiction,
  };

  explicit PotentialDisjointSetUnion(std::size_t element_count)
      : parent_(element_count), component_size_(element_count, 1U),
        delta_to_parent_(element_count, 0), min_potential_(element_count, 0),
        max_potential_(element_count, 0), components_(element_count) {
    std::iota(parent_.begin(), parent_.end(), std::size_t{0});
  }

  [[nodiscard]] std::size_t size() const noexcept { return parent_.size(); }
  [[nodiscard]] std::size_t components() const noexcept { return components_; }

  ConstraintResult constrain(std::size_t from, std::size_t to,
                             std::int64_t delta) {
    const FindResult first = find_with_potential(from);
    const FindResult second = find_with_potential(to);

    if (first.root == second.root) {
      try {
        const std::int64_t observed = checked_sub(second.potential_to_root,
                                                  first.potential_to_root);
        return observed == delta ? ConstraintResult::already_satisfied
                                 : ConstraintResult::contradiction;
      } catch (const std::overflow_error&) {
        // A mathematical difference outside int64_t cannot equal the supplied
        // int64_t constraint. The component is unchanged and the new
        // constraint is simply inconsistent with it.
        return ConstraintResult::contradiction;
      }
    }

    // If root_to is attached below root_from, this is
    // potential(root_to) - potential(root_from).
    const std::int64_t root_to_minus_root_from = checked_add(
        delta, checked_sub(first.potential_to_root,
                           second.potential_to_root));

    if (component_size_[first.root] >= component_size_[second.root]) {
      const std::int64_t shifted_min =
          checked_add(min_potential_[second.root], root_to_minus_root_from);
      const std::int64_t shifted_max =
          checked_add(max_potential_[second.root], root_to_minus_root_from);
      const std::int64_t merged_min =
          min_potential_[first.root] < shifted_min ? min_potential_[first.root]
                                                  : shifted_min;
      const std::int64_t merged_max =
          max_potential_[first.root] > shifted_max ? max_potential_[first.root]
                                                  : shifted_max;

      parent_[second.root] = first.root;
      delta_to_parent_[second.root] = root_to_minus_root_from;
      component_size_[first.root] += component_size_[second.root];
      min_potential_[first.root] = merged_min;
      max_potential_[first.root] = merged_max;
    } else {
      // Preflight every arithmetic operation before changing partition state.
      const std::int64_t root_from_minus_root_to =
          checked_negate(root_to_minus_root_from);
      const std::int64_t shifted_min =
          checked_add(min_potential_[first.root], root_from_minus_root_to);
      const std::int64_t shifted_max =
          checked_add(max_potential_[first.root], root_from_minus_root_to);
      const std::int64_t merged_min =
          min_potential_[second.root] < shifted_min ? min_potential_[second.root]
                                                   : shifted_min;
      const std::int64_t merged_max =
          max_potential_[second.root] > shifted_max ? max_potential_[second.root]
                                                   : shifted_max;

      parent_[first.root] = second.root;
      delta_to_parent_[first.root] = root_from_minus_root_to;
      component_size_[second.root] += component_size_[first.root];
      min_potential_[second.root] = merged_min;
      max_potential_[second.root] = merged_max;
    }
    --components_;
    return ConstraintResult::merged;
  }

  [[nodiscard]] std::optional<std::int64_t> difference(std::size_t from,
                                                        std::size_t to) {
    const FindResult first = find_with_potential(from);
    const FindResult second = find_with_potential(to);
    if (first.root != second.root) {
      return std::nullopt;
    }
    return checked_sub(second.potential_to_root, first.potential_to_root);
  }

  [[nodiscard]] bool connected(std::size_t a, std::size_t b) {
    return find_with_potential(a).root == find_with_potential(b).root;
  }

  [[nodiscard]] std::size_t component_size(std::size_t element) {
    const FindResult found = find_with_potential(element);
    return component_size_[found.root];
  }

  [[nodiscard]] bool valid_structure() const noexcept {
    if (parent_.size() != component_size_.size() ||
        parent_.size() != delta_to_parent_.size() ||
        parent_.size() != min_potential_.size() ||
        parent_.size() != max_potential_.size()) {
      return false;
    }

    std::vector<std::size_t> actual_sizes(parent_.size(), 0U);
    std::vector<std::int64_t> actual_min(parent_.size(), 0);
    std::vector<std::int64_t> actual_max(parent_.size(), 0);
    std::vector<bool> root_seen(parent_.size(), false);
    std::size_t roots = 0U;

    for (std::size_t element = 0; element < parent_.size(); ++element) {
      std::vector<std::size_t> path;
      std::size_t current = element;
      while (parent_[current] != current) {
        path.push_back(current);
        current = parent_[current];
        if (current >= parent_.size() || path.size() > parent_.size()) {
          return false;
        }
      }

      std::int64_t potential = 0;
      try {
        for (std::size_t reverse = path.size(); reverse > 0U; --reverse) {
          potential = checked_add(delta_to_parent_[path[reverse - 1U]],
                                  potential);
        }
      } catch (...) {
        return false;
      }

      ++actual_sizes[current];
      if (!root_seen[current]) {
        actual_min[current] = potential;
        actual_max[current] = potential;
        root_seen[current] = true;
      } else {
        if (potential < actual_min[current]) actual_min[current] = potential;
        if (potential > actual_max[current]) actual_max[current] = potential;
      }
    }

    for (std::size_t element = 0; element < parent_.size(); ++element) {
      if (parent_[element] == element) {
        ++roots;
        if (delta_to_parent_[element] != 0 ||
            component_size_[element] != actual_sizes[element] ||
            !root_seen[element] || min_potential_[element] != actual_min[element] ||
            max_potential_[element] != actual_max[element]) {
          return false;
        }
      }
    }
    return roots == components_;
  }

 private:
  struct FindResult {
    std::size_t root;
    std::int64_t potential_to_root;
  };

  static std::int64_t checked_add(std::int64_t left, std::int64_t right) {
    if ((right > 0 && left > std::numeric_limits<std::int64_t>::max() - right) ||
        (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right)) {
      throw std::overflow_error("potential DSU signed addition overflow");
    }
    return static_cast<std::int64_t>(left + right);
  }

  static std::int64_t checked_negate(std::int64_t value) {
    if (value == std::numeric_limits<std::int64_t>::min()) {
      throw std::overflow_error("potential DSU signed negation overflow");
    }
    return static_cast<std::int64_t>(-value);
  }

  static std::int64_t checked_sub(std::int64_t left, std::int64_t right) {
    if (right == std::numeric_limits<std::int64_t>::min()) {
      if (left >= 0) {
        throw std::overflow_error("potential DSU signed subtraction overflow");
      }
      return checked_add(left, std::numeric_limits<std::int64_t>::max()) + 1;
    }
    return checked_add(left, static_cast<std::int64_t>(-right));
  }

  void validate(std::size_t element) const {
    if (element >= parent_.size()) {
      throw std::out_of_range("potential DSU element out of range");
    }
  }

  FindResult find_with_potential(std::size_t element) {
    validate(element);

    std::vector<std::size_t> path;
    std::size_t current = element;
    while (parent_[current] != current) {
      path.push_back(current);
      current = parent_[current];
      if (path.size() > parent_.size()) {
        throw std::logic_error("potential DSU parent cycle");
      }
    }
    const std::size_t root = current;

    // Precompute every compressed potential before mutating parent links, so an
    // arithmetic failure cannot leave a partially compressed path.
    std::vector<std::int64_t> potentials(path.size(), 0);
    std::int64_t accumulated = 0;
    for (std::size_t reverse = path.size(); reverse > 0U; --reverse) {
      const std::size_t index = reverse - 1U;
      accumulated = checked_add(delta_to_parent_[path[index]], accumulated);
      potentials[index] = accumulated;
    }

    for (std::size_t index = 0; index < path.size(); ++index) {
      parent_[path[index]] = root;
      delta_to_parent_[path[index]] = potentials[index];
    }

    return FindResult{root, path.empty() ? 0 : potentials.front()};
  }

  std::vector<std::size_t> parent_;
  std::vector<std::size_t> component_size_;
  // potential(v) - potential(parent(v)). Roots always store zero.
  std::vector<std::int64_t> delta_to_parent_;
  // Meaningful at roots: exact extrema of potential(v)-potential(root).
  std::vector<std::int64_t> min_potential_;
  std::vector<std::int64_t> max_potential_;
  std::size_t components_;
};

}  // namespace algorithms::data_structures
