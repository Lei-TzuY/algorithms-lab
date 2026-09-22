#pragma once

#include "algorithms/graphs/graph.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

// Insertion-only undirected multigraph bridge / 2-edge-connectivity index.
//
// The structure maintains two disjoint-set layers:
//   - dsu_2ecc_: current 2-edge-connected components;
//   - dsu_cc_:   current connected components of the bridge forest.
//
// parent_ stores the current bridge forest between 2-edge-component
// representatives. Adding an edge between different connected components adds
// one bridge. Adding an edge inside one connected component finds the two paths
// to their first common ancestor and contracts exactly the bridges on the new
// cycle.
//
// Parallel edges and self-loops are supported. The second parallel edge closes a
// length-two multigraph cycle and therefore removes the original bridge.
class OnlineBridgeConnectivity {
 public:
  explicit OnlineBridgeConnectivity(const std::size_t vertex_count)
      : dsu_2ecc_(vertex_count),
        dsu_cc_(vertex_count),
        dsu_cc_size_(vertex_count, 1U),
        parent_(vertex_count, kNoVertex),
        last_visit_(vertex_count, 0U),
        path_first_(vertex_count, kNoVertex),
        path_second_(vertex_count, kNoVertex),
        component_count_(vertex_count) {
    for (Vertex vertex = 0U; vertex < vertex_count; ++vertex) {
      dsu_2ecc_[vertex] = vertex;
      dsu_cc_[vertex] = vertex;
    }
  }

  [[nodiscard]] std::size_t vertex_count() const noexcept {
    return dsu_2ecc_.size();
  }

  [[nodiscard]] std::size_t edge_count() const noexcept { return edge_count_; }
  [[nodiscard]] std::size_t bridge_count() const noexcept {
    return bridge_count_;
  }
  [[nodiscard]] std::size_t component_count() const noexcept {
    return component_count_;
  }

  [[nodiscard]] bool connected(const Vertex first,
                               const Vertex second) const {
    validate_vertex(first);
    validate_vertex(second);
    return find_cc_readonly(first) == find_cc_readonly(second);
  }

  [[nodiscard]] bool same_two_edge_component(const Vertex first,
                                             const Vertex second) const {
    validate_vertex(first);
    validate_vertex(second);
    return find_2ecc_readonly(first) == find_2ecc_readonly(second);
  }

  // Inserts one undirected multigraph edge and returns its monotone insertion id.
  //
  // Validation and edge-id exhaustion checks occur before semantic mutation.
  // After that point the update uses only constructor-allocated scratch state.
  [[nodiscard]] std::size_t add_edge(const Vertex first,
                                     const Vertex second) {
    validate_vertex(first);
    validate_vertex(second);
    if (edge_count_ == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error("online bridge edge id exhausted");
    }

    Vertex a = find_2ecc(first);
    Vertex b = find_2ecc(second);

    if (a != b) {
      Vertex component_a = find_cc(a);
      Vertex component_b = find_cc(b);

      if (component_a != component_b) {
        ++bridge_count_;
        --component_count_;

        if (dsu_cc_size_[component_a] > dsu_cc_size_[component_b]) {
          std::swap(a, b);
          std::swap(component_a, component_b);
        }

        make_root(a);
        parent_[a] = b;
        dsu_cc_[a] = b;
        dsu_cc_size_[component_b] += dsu_cc_size_[a];
      } else {
        merge_path(a, b);
      }
    }

    const std::size_t edge_id = edge_count_;
    ++edge_count_;
    return edge_id;
  }

  // Structural diagnostic for the two DSU layers and bridge forest. Semantic
  // bridge correctness is verified independently in tests by rebuilding Tarjan
  // low-link state from the external edge trace.
  [[nodiscard]] bool valid_invariants() const noexcept {
    try {
      const std::size_t count = vertex_count();
      if (dsu_cc_.size() != count || dsu_cc_size_.size() != count ||
          parent_.size() != count || last_visit_.size() != count ||
          path_first_.size() != count || path_second_.size() != count ||
          component_count_ > count ||
          bridge_count_ > count - component_count_) {
        return false;
      }

      std::vector<unsigned char> connected_roots(count, 0U);
      std::size_t observed_components = 0U;

      for (Vertex vertex = 0U; vertex < count; ++vertex) {
        if (dsu_2ecc_[vertex] >= count || dsu_cc_[vertex] >= count ||
            (parent_[vertex] != kNoVertex && parent_[vertex] >= count)) {
          return false;
        }

        const Vertex two_edge_root = find_2ecc_readonly(vertex);
        const Vertex cc_root = find_cc_readonly(vertex);
        if (two_edge_root == kNoVertex || cc_root == kNoVertex ||
            dsu_2ecc_[two_edge_root] != two_edge_root) {
          return false;
        }

        if (connected_roots[cc_root] == 0U) {
          connected_roots[cc_root] = 1U;
          ++observed_components;
          if (dsu_cc_size_[cc_root] == 0U) {
            return false;
          }
        }
      }

      if (observed_components != component_count_) {
        return false;
      }

      // Every bridge-forest parent must leave its current 2-edge component.
      for (Vertex vertex = 0U; vertex < count; ++vertex) {
        const Vertex representative = find_2ecc_readonly(vertex);
        if (representative != vertex || parent_[vertex] == kNoVertex) {
          continue;
        }
        const Vertex parent_rep = find_2ecc_readonly(parent_[vertex]);
        if (parent_rep == kNoVertex || parent_rep == representative ||
            find_cc_readonly(parent_rep) != find_cc_readonly(representative)) {
          return false;
        }
      }

      return true;
    } catch (...) {
      return false;
    }
  }

 private:
  static constexpr Vertex kNoVertex =
      std::numeric_limits<Vertex>::max();

  std::vector<Vertex> dsu_2ecc_;
  std::vector<Vertex> dsu_cc_;
  std::vector<std::size_t> dsu_cc_size_;
  std::vector<Vertex> parent_;
  std::vector<std::size_t> last_visit_;
  std::vector<Vertex> path_first_;
  std::vector<Vertex> path_second_;

  std::size_t visit_epoch_{};
  std::size_t edge_count_{};
  std::size_t bridge_count_{};
  std::size_t component_count_{};

  void validate_vertex(const Vertex vertex) const {
    if (vertex >= vertex_count()) {
      throw std::out_of_range("online bridge vertex out of range");
    }
  }

  [[nodiscard]] Vertex find_2ecc(Vertex vertex) {
    if (vertex == kNoVertex) {
      return kNoVertex;
    }

    Vertex root = vertex;
    std::size_t hops = 0U;
    while (dsu_2ecc_[root] != root) {
      root = dsu_2ecc_[root];
      if (++hops > vertex_count()) {
        throw std::logic_error("online bridge 2ECC DSU cycle");
      }
    }

    while (dsu_2ecc_[vertex] != vertex) {
      const Vertex next = dsu_2ecc_[vertex];
      dsu_2ecc_[vertex] = root;
      vertex = next;
    }
    return root;
  }

  [[nodiscard]] Vertex find_cc(Vertex vertex) {
    vertex = find_2ecc(vertex);
    Vertex root = vertex;
    std::size_t hops = 0U;

    while (true) {
      const Vertex next = find_2ecc(dsu_cc_[root]);
      if (next == root) {
        break;
      }
      root = next;
      if (++hops > vertex_count()) {
        throw std::logic_error("online bridge CC DSU cycle");
      }
    }

    Vertex cursor = vertex;
    while (cursor != root) {
      const Vertex next = find_2ecc(dsu_cc_[cursor]);
      dsu_cc_[cursor] = root;
      cursor = next;
    }
    return root;
  }

  [[nodiscard]] Vertex find_2ecc_readonly(Vertex vertex) const {
    if (vertex == kNoVertex) {
      return kNoVertex;
    }

    std::size_t hops = 0U;
    while (vertex < vertex_count() && dsu_2ecc_[vertex] != vertex) {
      vertex = dsu_2ecc_[vertex];
      if (++hops > vertex_count()) {
        return kNoVertex;
      }
    }
    return vertex < vertex_count() ? vertex : kNoVertex;
  }

  [[nodiscard]] Vertex find_cc_readonly(Vertex vertex) const {
    vertex = find_2ecc_readonly(vertex);
    if (vertex == kNoVertex) {
      return kNoVertex;
    }

    std::size_t hops = 0U;
    while (true) {
      const Vertex raw = dsu_cc_[vertex];
      const Vertex next = find_2ecc_readonly(raw);
      if (next == kNoVertex) {
        return kNoVertex;
      }
      if (next == vertex) {
        return vertex;
      }
      vertex = next;
      if (++hops > vertex_count()) {
        return kNoVertex;
      }
    }
  }

  void make_root(Vertex vertex) {
    vertex = find_2ecc(vertex);
    const Vertex root = vertex;
    Vertex child = kNoVertex;

    while (vertex != kNoVertex) {
      const Vertex next = find_2ecc(parent_[vertex]);
      parent_[vertex] = child;
      dsu_cc_[vertex] = root;
      child = vertex;
      vertex = next;
    }

    if (child == kNoVertex) {
      throw std::logic_error("online bridge reroot lost component");
    }
    dsu_cc_size_[root] = dsu_cc_size_[child];
  }

  void begin_visit_epoch() noexcept {
    if (visit_epoch_ == std::numeric_limits<std::size_t>::max()) {
      std::fill(last_visit_.begin(), last_visit_.end(), 0U);
      visit_epoch_ = 1U;
      return;
    }
    ++visit_epoch_;
  }

  void merge_path(Vertex first, Vertex second) {
    begin_visit_epoch();

    Vertex left = first;
    Vertex right = second;
    std::size_t left_length = 0U;
    std::size_t right_length = 0U;
    bool take_left = true;
    Vertex lca = kNoVertex;
    std::size_t rounds = 0U;

    while (lca == kNoVertex) {
      Vertex& current = take_left ? left : right;
      std::vector<Vertex>& path = take_left ? path_first_ : path_second_;
      std::size_t& length = take_left ? left_length : right_length;

      if (current != kNoVertex) {
        current = find_2ecc(current);
        if (length >= vertex_count()) {
          throw std::logic_error("online bridge path scratch exhausted");
        }
        path[length++] = current;

        if (last_visit_[current] == visit_epoch_) {
          lca = current;
          break;
        }
        last_visit_[current] = visit_epoch_;
        current = parent_[current];
      }

      take_left = !take_left;
      if (++rounds > 2U * vertex_count() + 2U) {
        throw std::logic_error("online bridge LCA search failed");
      }
    }

    std::size_t removed_bridges = 0U;
    for (std::size_t index = 0U; index < left_length; ++index) {
      if (path_first_[index] == lca) {
        break;
      }
      ++removed_bridges;
    }
    for (std::size_t index = 0U; index < right_length; ++index) {
      if (path_second_[index] == lca) {
        break;
      }
      ++removed_bridges;
    }
    if (removed_bridges > bridge_count_) {
      throw std::logic_error("online bridge count underflow");
    }

    for (std::size_t index = 0U; index < left_length; ++index) {
      const Vertex vertex = path_first_[index];
      dsu_2ecc_[vertex] = lca;
      if (vertex == lca) {
        break;
      }
    }
    for (std::size_t index = 0U; index < right_length; ++index) {
      const Vertex vertex = path_second_[index];
      dsu_2ecc_[vertex] = lca;
      if (vertex == lca) {
        break;
      }
    }
    bridge_count_ -= removed_bridges;
  }
};

}  // namespace algorithms::graphs
