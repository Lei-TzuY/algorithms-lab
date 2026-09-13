#pragma once

#include "algorithms/data_structures/euler_tour_forest.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>

namespace algorithms::data_structures {

class FullyDynamicConnectivity {
 public:
  explicit FullyDynamicConnectivity(
      std::size_t vertex_count,
      std::uint64_t forest_seed = 0xF0117D1A6C0ULL)
      : forest_(vertex_count, forest_seed) {}

  [[nodiscard]] std::size_t vertex_count() const noexcept {
    return forest_.vertex_count();
  }
  [[nodiscard]] std::size_t edge_count() const noexcept { return edge_count_; }
  [[nodiscard]] std::size_t tree_edge_count() const noexcept {
    return forest_.edge_count();
  }
  [[nodiscard]] std::size_t non_tree_edge_count() const noexcept {
    return edge_count_ - forest_.edge_count();
  }
  [[nodiscard]] bool connected(std::size_t first, std::size_t second) const {
    return forest_.connected(first, second);
  }

  [[nodiscard]] std::size_t component_size(std::size_t vertex) const {
    return forest_.component_size(vertex);
  }

  [[nodiscard]] std::size_t multiplicity(std::size_t first,
                                         std::size_t second) const {
    validate_vertex(first);
    validate_vertex(second);
    const auto it = edges_.find(canonical_key(first, second));
    return it == edges_.end() ? 0U : it->second.multiplicity;
  }

  void add_edge(std::size_t first, std::size_t second) {
    validate_vertex(first);
    validate_vertex(second);
    if (edge_count_ == std::numeric_limits<std::size_t>::max()) {
      throw std::overflow_error("FullyDynamicConnectivity edge count overflow");
    }

    const EdgeKey key = canonical_key(first, second);
    auto [it, inserted] = edges_.try_emplace(key, EdgeRecord{});
    EdgeRecord& record = it->second;
    if (record.multiplicity == std::numeric_limits<std::size_t>::max()) {
      if (inserted) {
        edges_.erase(it);
      }
      throw std::overflow_error("FullyDynamicConnectivity multiplicity overflow");
    }

    ++record.multiplicity;
    ++edge_count_;
    if (first == second) {
      ++record.non_tree_copies;
      return;
    }

    if (!forest_.connected(first, second)) {
      if (record.has_tree_copy) {
        throw std::logic_error("dynamic-connectivity tree bookkeeping mismatch");
      }
      forest_.link(first, second);
      record.has_tree_copy = true;
    } else {
      ++record.non_tree_copies;
    }
  }

  void remove_edge(std::size_t first, std::size_t second) {
    validate_vertex(first);
    validate_vertex(second);
    const EdgeKey key = canonical_key(first, second);
    auto it = edges_.find(key);
    if (it == edges_.end() || it->second.multiplicity == 0) {
      throw std::invalid_argument(
          "FullyDynamicConnectivity remove requires an active edge");
    }

    EdgeRecord& record = it->second;
    if (first == second) {
      --record.non_tree_copies;
      --record.multiplicity;
      --edge_count_;
      if (record.multiplicity == 0) {
        edges_.erase(it);
      }
      return;
    }

    if (record.non_tree_copies != 0) {
      --record.non_tree_copies;
      --record.multiplicity;
      --edge_count_;
      if (record.multiplicity == 0) {
        if (record.has_tree_copy) {
          throw std::logic_error("dynamic-connectivity empty tree record");
        }
        edges_.erase(it);
      }
      return;
    }

    if (!record.has_tree_copy || record.multiplicity != 1) {
      throw std::logic_error("dynamic-connectivity tree multiplicity mismatch");
    }

    forest_.cut(first, second);
    record.has_tree_copy = false;
    --record.multiplicity;
    --edge_count_;
    edges_.erase(it);
    promote_replacement_if_available();
  }

  [[nodiscard]] bool valid_structure() const {
    if (!forest_.valid_structure() || forest_.edge_count() > edge_count_) {
      return false;
    }
    std::size_t exact_edges = 0;
    std::size_t exact_tree_edges = 0;
    for (const auto& [key, record] : edges_) {
      if (record.multiplicity == 0 ||
          record.non_tree_copies > record.multiplicity) {
        return false;
      }
      if (key.first == key.second) {
        if (record.has_tree_copy ||
            record.non_tree_copies != record.multiplicity) {
          return false;
        }
      } else {
        const std::size_t expected_non_tree =
            record.multiplicity - (record.has_tree_copy ? 1U : 0U);
        if (record.non_tree_copies != expected_non_tree) {
          return false;
        }
        if (!forest_.connected(key.first, key.second)) {
          return false;
        }
        if (record.has_tree_copy) {
          ++exact_tree_edges;
        }
      }
      if (exact_edges >
          std::numeric_limits<std::size_t>::max() - record.multiplicity) {
        return false;
      }
      exact_edges += record.multiplicity;
    }
    return exact_edges == edge_count_ && exact_tree_edges == forest_.edge_count();
  }

 private:
  struct EdgeKey {
    std::size_t first = 0;
    std::size_t second = 0;
    friend bool operator<(const EdgeKey& lhs, const EdgeKey& rhs) noexcept {
      return lhs.first < rhs.first ||
             (lhs.first == rhs.first && lhs.second < rhs.second);
    }
  };
  struct EdgeRecord {
    std::size_t multiplicity = 0;
    std::size_t non_tree_copies = 0;
    bool has_tree_copy = false;
  };

  [[nodiscard]] static EdgeKey canonical_key(std::size_t first,
                                              std::size_t second) noexcept {
    return first <= second ? EdgeKey{first, second}
                           : EdgeKey{second, first};
  }

  void validate_vertex(std::size_t vertex) const {
    if (vertex >= forest_.vertex_count()) {
      throw std::out_of_range("FullyDynamicConnectivity vertex out of range");
    }
  }

  void promote_replacement_if_available() {
    for (auto& [key, record] : edges_) {
      if (key.first == key.second || record.non_tree_copies == 0) {
        continue;
      }
      if (!forest_.connected(key.first, key.second)) {
        forest_.link(key.first, key.second);
        record.has_tree_copy = true;
        --record.non_tree_copies;
        return;
      }
    }
  }

  EulerTourForest forest_;
  std::map<EdgeKey, EdgeRecord> edges_;
  std::size_t edge_count_ = 0;
};

}  // namespace algorithms::data_structures
