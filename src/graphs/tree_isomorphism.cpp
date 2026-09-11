#include "algorithms/graphs/tree_isomorphism.hpp"

#include <algorithm>
#include <map>
#include <numeric>
#include <queue>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

struct ValidatedTree {
  std::vector<Vertex> centers;
};

ValidatedTree validate_tree(const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument("tree isomorphism requires undirected graphs");
  }

  const std::size_t n = graph.vertex_count();
  if (n == 0U) {
    return {};
  }

  std::set<std::pair<Vertex, Vertex>> logical_edges;
  for (Vertex from = 0; from < n; ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      if (edge.to == from) {
        throw std::invalid_argument("tree isomorphism rejects self-loops");
      }
      const auto canonical = std::minmax(from, edge.to);
      if (from < edge.to && !logical_edges.insert(canonical).second) {
        throw std::invalid_argument("tree isomorphism rejects parallel edges");
      }
    }
  }

  if (logical_edges.size() != n - 1U) {
    throw std::invalid_argument("tree isomorphism requires exactly V-1 edges");
  }

  std::vector<unsigned char> seen(n, 0U);
  std::vector<Vertex> stack{0U};
  seen[0] = 1U;
  std::size_t visited = 0U;
  while (!stack.empty()) {
    const Vertex vertex = stack.back();
    stack.pop_back();
    ++visited;
    for (const Edge& edge : graph.neighbors(vertex)) {
      if (seen[edge.to] == 0U) {
        seen[edge.to] = 1U;
        stack.push_back(edge.to);
      }
    }
  }
  if (visited != n) {
    throw std::invalid_argument("tree isomorphism requires connected input");
  }

  if (n == 1U) {
    return {{0U}};
  }

  std::vector<std::size_t> degree(n, 0U);
  std::queue<Vertex> leaves;
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    degree[vertex] = graph.neighbors(vertex).size();
    if (degree[vertex] <= 1U) {
      leaves.push(vertex);
    }
  }

  std::size_t remaining = n;
  while (remaining > 2U) {
    const std::size_t layer_size = leaves.size();
    if (layer_size == 0U || layer_size > remaining) {
      throw std::logic_error("invalid tree center peeling state");
    }
    remaining -= layer_size;
    for (std::size_t index = 0; index < layer_size; ++index) {
      const Vertex leaf = leaves.front();
      leaves.pop();
      for (const Edge& edge : graph.neighbors(leaf)) {
        if (degree[edge.to] == 0U) {
          continue;
        }
        --degree[edge.to];
        if (degree[edge.to] == 1U) {
          leaves.push(edge.to);
        }
      }
      degree[leaf] = 0U;
    }
  }

  std::vector<Vertex> centers;
  while (!leaves.empty()) {
    centers.push_back(leaves.front());
    leaves.pop();
  }
  std::sort(centers.begin(), centers.end());
  if (centers.size() != remaining || centers.empty() || centers.size() > 2U) {
    throw std::logic_error("invalid tree center witness");
  }
  return {std::move(centers)};
}

struct RootedTree {
  Vertex root{0U};
  std::vector<Vertex> parent;
  std::vector<std::vector<Vertex>> children;
  std::vector<Vertex> postorder;
  std::vector<std::size_t> signature;
};

RootedTree root_tree(const Graph& graph, Vertex root) {
  const std::size_t n = graph.vertex_count();
  RootedTree rooted;
  rooted.root = root;
  rooted.parent.assign(n, n);
  rooted.children.resize(n);
  rooted.signature.assign(n, 0U);

  std::vector<Vertex> order;
  order.reserve(n);
  rooted.parent[root] = root;
  order.push_back(root);
  for (std::size_t index = 0; index < order.size(); ++index) {
    const Vertex vertex = order[index];
    for (const Edge& edge : graph.neighbors(vertex)) {
      if (edge.to == rooted.parent[vertex]) {
        continue;
      }
      if (rooted.parent[edge.to] != n) {
        throw std::logic_error("validated tree acquired a cycle while rooting");
      }
      rooted.parent[edge.to] = vertex;
      rooted.children[vertex].push_back(edge.to);
      order.push_back(edge.to);
    }
  }
  if (order.size() != n) {
    throw std::logic_error("validated tree became disconnected while rooting");
  }
  rooted.postorder.assign(order.rbegin(), order.rend());
  return rooted;
}

class SignatureInterner {
 public:
  std::size_t intern(std::vector<std::size_t> children) {
    std::sort(children.begin(), children.end());
    const auto found = ids_.find(children);
    if (found != ids_.end()) {
      return found->second;
    }
    const std::size_t id = ids_.size() + 1U;
    ids_.emplace(std::move(children), id);
    return id;
  }

 private:
  std::map<std::vector<std::size_t>, std::size_t> ids_;
};

void label_rooted_tree(RootedTree& tree, SignatureInterner& interner) {
  for (const Vertex vertex : tree.postorder) {
    std::vector<std::size_t> child_signatures;
    child_signatures.reserve(tree.children[vertex].size());
    for (const Vertex child : tree.children[vertex]) {
      child_signatures.push_back(tree.signature[child]);
    }
    tree.signature[vertex] = interner.intern(std::move(child_signatures));
  }
}

std::optional<TreeIsomorphismResult> try_root_pair(
    const Graph& first, const Graph& second,
    const std::vector<Vertex>& first_centers,
    const std::vector<Vertex>& second_centers, Vertex first_root,
    Vertex second_root) {
  RootedTree rooted_first = root_tree(first, first_root);
  RootedTree rooted_second = root_tree(second, second_root);
  SignatureInterner interner;
  label_rooted_tree(rooted_first, interner);
  label_rooted_tree(rooted_second, interner);
  if (rooted_first.signature[first_root] != rooted_second.signature[second_root]) {
    return std::nullopt;
  }

  const std::size_t n = first.vertex_count();
  TreeIsomorphismResult result;
  result.first_to_second.assign(n, n);
  result.second_to_first.assign(n, n);
  result.first_centers = first_centers;
  result.second_centers = second_centers;

  std::vector<std::pair<Vertex, Vertex>> pending{{first_root, second_root}};
  while (!pending.empty()) {
    const auto [left, right] = pending.back();
    pending.pop_back();
    if (result.first_to_second[left] != n || result.second_to_first[right] != n) {
      throw std::logic_error("tree isomorphism witness assigned a vertex twice");
    }
    result.first_to_second[left] = right;
    result.second_to_first[right] = left;

    std::map<std::size_t, std::vector<Vertex>> left_groups;
    std::map<std::size_t, std::vector<Vertex>> right_groups;
    for (const Vertex child : rooted_first.children[left]) {
      left_groups[rooted_first.signature[child]].push_back(child);
    }
    for (const Vertex child : rooted_second.children[right]) {
      right_groups[rooted_second.signature[child]].push_back(child);
    }
    if (left_groups.size() != right_groups.size()) {
      throw std::logic_error("equal rooted signatures produced different child groups");
    }
    for (auto& [signature, left_children] : left_groups) {
      const auto found = right_groups.find(signature);
      if (found == right_groups.end() || found->second.size() != left_children.size()) {
        throw std::logic_error("equal rooted signatures produced incompatible groups");
      }
      std::vector<Vertex> right_children = found->second;
      std::sort(left_children.begin(), left_children.end());
      std::sort(right_children.begin(), right_children.end());
      for (std::size_t index = 0; index < left_children.size(); ++index) {
        pending.emplace_back(left_children[index], right_children[index]);
      }
    }
  }

  return result;
}

}  // namespace

std::optional<TreeIsomorphismResult> exact_tree_isomorphism(
    const Graph& first, const Graph& second) {
  const ValidatedTree first_tree = validate_tree(first);
  const ValidatedTree second_tree = validate_tree(second);
  if (first.vertex_count() != second.vertex_count()) {
    return std::nullopt;
  }
  if (first.vertex_count() == 0U) {
    return TreeIsomorphismResult{};
  }
  if (first_tree.centers.size() != second_tree.centers.size()) {
    return std::nullopt;
  }

  const Vertex first_root = first_tree.centers.front();
  for (const Vertex second_root : second_tree.centers) {
    auto result = try_root_pair(first, second, first_tree.centers,
                                second_tree.centers, first_root, second_root);
    if (result.has_value()) {
      return result;
    }
  }
  return std::nullopt;
}

}  // namespace algorithms::graphs
