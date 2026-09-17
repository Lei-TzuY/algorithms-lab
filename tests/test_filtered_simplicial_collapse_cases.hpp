#pragma once

#include "algorithms/topology/filtered_simplicial_collapse.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <span>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace filtered_collapse_test_detail {

using algorithms::topology::ElementaryCollapsePair;
using algorithms::topology::FilteredSimplex;
using algorithms::topology::FilteredSimplicialCollapseResult;
using algorithms::topology::PersistenceInterval;
using algorithms::topology::filtration_preserving_elementary_collapse;
using algorithms::topology::persistent_homology_z2;

inline std::vector<std::size_t> canonical_vertices(std::vector<std::size_t> vertices) {
  std::sort(vertices.begin(), vertices.end());
  return vertices;
}

inline bool is_immediate_face(const std::vector<std::size_t>& face,
                              const std::vector<std::size_t>& coface) {
  return coface.size() == face.size() + 1 &&
         std::includes(coface.begin(), coface.end(), face.begin(), face.end());
}

inline bool output_order(const FilteredSimplex& left, const FilteredSimplex& right) {
  if (left.filtration != right.filtration) {
    return left.filtration < right.filtration;
  }
  if (left.vertices.size() != right.vertices.size()) {
    return left.vertices.size() < right.vertices.size();
  }
  return left.vertices < right.vertices;
}

inline std::vector<FilteredSimplex> canonical_complex(
    std::span<const FilteredSimplex> input) {
  std::vector<FilteredSimplex> result;
  result.reserve(input.size());
  for (const auto& simplex : input) {
    result.push_back({canonical_vertices(simplex.vertices), simplex.filtration});
  }
  std::sort(result.begin(), result.end(), output_order);
  return result;
}

inline void verify_replay(std::span<const FilteredSimplex> input,
                          const FilteredSimplicialCollapseResult& result) {
  std::map<std::vector<std::size_t>, std::int64_t> active;
  for (const auto& simplex : canonical_complex(input)) {
    REQUIRE(active.emplace(simplex.vertices, simplex.filtration).second);
  }

  for (const ElementaryCollapsePair& collapse : result.collapses) {
    const auto face = active.find(collapse.free_face.vertices);
    const auto coface = active.find(collapse.coface.vertices);
    REQUIRE(face != active.end());
    REQUIRE(coface != active.end());
    REQUIRE_EQ(face->second, collapse.free_face.filtration);
    REQUIRE_EQ(coface->second, collapse.coface.filtration);
    REQUIRE_EQ(face->second, coface->second);
    REQUIRE(is_immediate_face(collapse.free_face.vertices, collapse.coface.vertices));

    std::vector<std::vector<std::size_t>> active_cofaces;
    for (const auto& [vertices, filtration] : active) {
      static_cast<void>(filtration);
      if (is_immediate_face(collapse.free_face.vertices, vertices)) {
        active_cofaces.push_back(vertices);
      }
    }
    REQUIRE_EQ(active_cofaces.size(), std::size_t{1});
    REQUIRE_EQ(active_cofaces.front(), collapse.coface.vertices);
    active.erase(face);
    active.erase(coface);
  }

  std::vector<FilteredSimplex> replayed;
  replayed.reserve(active.size());
  for (const auto& [vertices, filtration] : active) {
    replayed.push_back({vertices, filtration});
  }
  std::sort(replayed.begin(), replayed.end(), output_order);
  REQUIRE_EQ(replayed, result.reduced_simplices);
}

inline std::size_t gf2_rank(std::vector<std::vector<unsigned char>> matrix) {
  if (matrix.empty()) {
    return 0;
  }
  const std::size_t rows = matrix.size();
  const std::size_t columns = matrix.front().size();
  std::size_t rank = 0;
  for (std::size_t column = 0; column < columns && rank < rows; ++column) {
    std::size_t pivot = rank;
    while (pivot < rows && matrix[pivot][column] == 0) {
      ++pivot;
    }
    if (pivot == rows) {
      continue;
    }
    std::swap(matrix[pivot], matrix[rank]);
    for (std::size_t row = 0; row < rows; ++row) {
      if (row != rank && matrix[row][column] != 0) {
        for (std::size_t current = column; current < columns; ++current) {
          matrix[row][current] ^= matrix[rank][current];
        }
      }
    }
    ++rank;
  }
  return rank;
}

inline std::vector<std::size_t> betti_at(
    std::span<const FilteredSimplex> input, std::int64_t threshold) {
  const auto canonical = canonical_complex(input);
  std::vector<FilteredSimplex> active;
  for (const auto& simplex : canonical) {
    if (simplex.filtration <= threshold) {
      active.push_back(simplex);
    }
  }
  if (active.empty()) {
    return {};
  }

  std::size_t max_dimension = 0;
  std::map<std::vector<std::size_t>, std::size_t> index_by_vertices;
  for (std::size_t index = 0; index < active.size(); ++index) {
    index_by_vertices.emplace(active[index].vertices, index);
    max_dimension = std::max(max_dimension, active[index].vertices.size() - 1);
  }

  std::vector<std::vector<std::size_t>> by_dimension(max_dimension + 1);
  for (std::size_t index = 0; index < active.size(); ++index) {
    by_dimension[active[index].vertices.size() - 1].push_back(index);
  }
  std::vector<std::size_t> boundary_ranks(max_dimension + 2, 0);
  for (std::size_t dimension = 1; dimension <= max_dimension; ++dimension) {
    const auto& domain = by_dimension[dimension];
    const auto& codomain = by_dimension[dimension - 1];
    std::map<std::size_t, std::size_t> row_of;
    for (std::size_t row = 0; row < codomain.size(); ++row) {
      row_of.emplace(codomain[row], row);
    }
    std::vector<std::vector<unsigned char>> matrix(
        codomain.size(), std::vector<unsigned char>(domain.size(), 0));
    for (std::size_t column = 0; column < domain.size(); ++column) {
      const auto& simplex = active[domain[column]].vertices;
      for (std::size_t removed = 0; removed < simplex.size(); ++removed) {
        std::vector<std::size_t> face;
        face.reserve(simplex.size() - 1);
        for (std::size_t position = 0; position < simplex.size(); ++position) {
          if (position != removed) {
            face.push_back(simplex[position]);
          }
        }
        const auto found = index_by_vertices.find(face);
        REQUIRE(found != index_by_vertices.end());
        matrix[row_of.at(found->second)][column] ^= 1;
      }
    }
    boundary_ranks[dimension] = gf2_rank(std::move(matrix));
  }

  std::vector<std::size_t> betti(max_dimension + 1, 0);
  for (std::size_t dimension = 0; dimension <= max_dimension; ++dimension) {
    betti[dimension] = by_dimension[dimension].size() - boundary_ranks[dimension] -
                       boundary_ranks[dimension + 1];
  }
  while (!betti.empty() && betti.back() == 0) {
    betti.pop_back();
  }
  return betti;
}

using ObservableInterval =
    std::tuple<std::size_t, std::int64_t, std::optional<std::int64_t>>;

inline std::vector<ObservableInterval> observable_barcode(
    std::span<const FilteredSimplex> complex) {
  const auto result = persistent_homology_z2(complex);
  std::vector<ObservableInterval> intervals;
  for (const PersistenceInterval& interval : result.intervals) {
    if (interval.death.has_value() && *interval.death == interval.birth) {
      continue;
    }
    intervals.emplace_back(interval.dimension, interval.birth, interval.death);
  }
  std::sort(intervals.begin(), intervals.end());
  return intervals;
}

}  // namespace filtered_collapse_test_detail

TEST_CASE(filtered_simplicial_collapse_validation_and_filtration_boundary) {
  using namespace filtered_collapse_test_detail;
  REQUIRE(filtration_preserving_elementary_collapse({}).reduced_simplices.empty());
  REQUIRE_THROWS_AS(
      filtration_preserving_elementary_collapse(
          std::vector<FilteredSimplex>{{{}, 0}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      filtration_preserving_elementary_collapse(
          std::vector<FilteredSimplex>{{{0, 0}, 0}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      filtration_preserving_elementary_collapse(
          std::vector<FilteredSimplex>{{{0}, 0}, {{0}, 1}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      filtration_preserving_elementary_collapse(
          std::vector<FilteredSimplex>{{{0}, 0}, {{0, 1}, 0}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      filtration_preserving_elementary_collapse(
          std::vector<FilteredSimplex>{{{0}, 1}, {{1}, 0}, {{0, 1}, 0}}),
      std::invalid_argument);

  const std::vector<FilteredSimplex> delayed_edge{
      {{0}, 0}, {{1}, 0}, {{0, 1}, 1}};
  const auto delayed = filtration_preserving_elementary_collapse(delayed_edge);
  REQUIRE(delayed.collapses.empty());
  REQUIRE_EQ(delayed.reduced_simplices.size(), delayed_edge.size());
}

TEST_CASE(filtered_simplicial_collapse_deterministic_shape_witnesses) {
  using namespace filtered_collapse_test_detail;
  const std::vector<FilteredSimplex> filled_triangle{
      {{0}, 0}, {{1}, 0}, {{2}, 0}, {{0, 1}, 0},
      {{0, 2}, 0}, {{1, 2}, 0}, {{0, 1, 2}, 0}};
  const auto triangle = filtration_preserving_elementary_collapse(filled_triangle);
  verify_replay(filled_triangle, triangle);
  REQUIRE_EQ(triangle.collapses.size(), std::size_t{3});
  REQUIRE_EQ(triangle.reduced_simplices.size(), std::size_t{1});

  const std::vector<FilteredSimplex> circle{
      {{0}, 0}, {{1}, 0}, {{2}, 0}, {{0, 1}, 0}, {{0, 2}, 0}, {{1, 2}, 0}};
  const auto circle_result = filtration_preserving_elementary_collapse(circle);
  verify_replay(circle, circle_result);
  REQUIRE(circle_result.collapses.empty());

  std::vector<FilteredSimplex> tetrahedron_boundary;
  for (std::size_t vertex = 0; vertex < 4; ++vertex) {
    tetrahedron_boundary.push_back({{vertex}, 0});
  }
  for (std::size_t left = 0; left < 4; ++left) {
    for (std::size_t right = left + 1; right < 4; ++right) {
      tetrahedron_boundary.push_back({{left, right}, 0});
    }
  }
  tetrahedron_boundary.push_back({{0, 1, 2}, 0});
  tetrahedron_boundary.push_back({{0, 1, 3}, 0});
  tetrahedron_boundary.push_back({{0, 2, 3}, 0});
  tetrahedron_boundary.push_back({{1, 2, 3}, 0});
  const auto sphere = filtration_preserving_elementary_collapse(tetrahedron_boundary);
  verify_replay(tetrahedron_boundary, sphere);
  REQUIRE(sphere.collapses.empty());
}

TEST_CASE(filtered_simplicial_collapse_zero_lifetime_barcode_boundary) {
  using namespace filtered_collapse_test_detail;
  const std::vector<FilteredSimplex> complex{
      {{0}, 0}, {{1}, 1}, {{0, 1}, 1}};
  const auto before = persistent_homology_z2(complex);
  bool has_zero_lifetime_pair = false;
  for (const auto& interval : before.intervals) {
    if (interval.death.has_value() && *interval.death == interval.birth) {
      has_zero_lifetime_pair = true;
    }
  }
  REQUIRE(has_zero_lifetime_pair);

  const auto reduced = filtration_preserving_elementary_collapse(complex);
  verify_replay(complex, reduced);
  REQUIRE_EQ(reduced.collapses.size(), std::size_t{1});
  REQUIRE_EQ(observable_barcode(complex), observable_barcode(reduced.reduced_simplices));
}

TEST_CASE(filtered_simplicial_collapse_randomized_persistence_differential) {
  using namespace filtered_collapse_test_detail;
  std::mt19937_64 random(0xC011A95E5EEDULL);
  std::uniform_int_distribution<int> vertex_count_distribution(0, 6);
  std::uniform_int_distribution<int> base_filtration_distribution(0, 2);
  std::bernoulli_distribution include_edge(0.58);
  std::bernoulli_distribution include_triangle(0.38);
  std::bernoulli_distribution include_tetrahedron(0.18);
  std::bernoulli_distribution add_delay(0.35);
  std::size_t total_collapses = 0;

  for (int trial = 0; trial < 500; ++trial) {
    const std::size_t vertex_count =
        static_cast<std::size_t>(vertex_count_distribution(random));
    std::vector<FilteredSimplex> complex;
    std::vector<std::int64_t> vertex_filtration(vertex_count, 0);
    for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
      vertex_filtration[vertex] = base_filtration_distribution(random);
      complex.push_back({{vertex}, vertex_filtration[vertex]});
    }

    std::map<std::pair<std::size_t, std::size_t>, std::int64_t> edge_filtration;
    for (std::size_t left = 0; left < vertex_count; ++left) {
      for (std::size_t right = left + 1; right < vertex_count; ++right) {
        if (!include_edge(random)) {
          continue;
        }
        const std::int64_t filtration =
            std::max(vertex_filtration[left], vertex_filtration[right]) +
            (add_delay(random) ? 1 : 0);
        edge_filtration.emplace(std::pair{left, right}, filtration);
        complex.push_back({{left, right}, filtration});
      }
    }

    std::map<std::tuple<std::size_t, std::size_t, std::size_t>, std::int64_t>
        triangle_filtration;
    for (std::size_t a = 0; a < vertex_count; ++a) {
      for (std::size_t b = a + 1; b < vertex_count; ++b) {
        for (std::size_t c = b + 1; c < vertex_count; ++c) {
          const auto ab = edge_filtration.find({a, b});
          const auto ac = edge_filtration.find({a, c});
          const auto bc = edge_filtration.find({b, c});
          if (ab == edge_filtration.end() || ac == edge_filtration.end() ||
              bc == edge_filtration.end() || !include_triangle(random)) {
            continue;
          }
          const std::int64_t filtration =
              std::max({ab->second, ac->second, bc->second}) +
              (add_delay(random) ? 1 : 0);
          triangle_filtration.emplace(std::tuple{a, b, c}, filtration);
          complex.push_back({{a, b, c}, filtration});
        }
      }
    }

    for (std::size_t a = 0; a < vertex_count; ++a) {
      for (std::size_t b = a + 1; b < vertex_count; ++b) {
        for (std::size_t c = b + 1; c < vertex_count; ++c) {
          for (std::size_t d = c + 1; d < vertex_count; ++d) {
            const auto abc = triangle_filtration.find({a, b, c});
            const auto abd = triangle_filtration.find({a, b, d});
            const auto acd = triangle_filtration.find({a, c, d});
            const auto bcd = triangle_filtration.find({b, c, d});
            if (abc == triangle_filtration.end() || abd == triangle_filtration.end() ||
                acd == triangle_filtration.end() || bcd == triangle_filtration.end() ||
                !include_tetrahedron(random)) {
              continue;
            }
            const std::int64_t filtration =
                std::max({abc->second, abd->second, acd->second, bcd->second}) +
                (add_delay(random) ? 1 : 0);
            complex.push_back({{a, b, c, d}, filtration});
          }
        }
      }
    }

    std::shuffle(complex.begin(), complex.end(), random);
    const auto reduced = filtration_preserving_elementary_collapse(complex);
    verify_replay(complex, reduced);
    total_collapses += reduced.collapses.size();

    std::set<std::int64_t> thresholds;
    for (const auto& simplex : complex) {
      thresholds.insert(simplex.filtration);
    }
    for (const auto& simplex : reduced.reduced_simplices) {
      thresholds.insert(simplex.filtration);
    }
    for (const std::int64_t threshold : thresholds) {
      REQUIRE_EQ(betti_at(complex, threshold),
                 betti_at(reduced.reduced_simplices, threshold));
    }
    REQUIRE_EQ(observable_barcode(complex),
               observable_barcode(reduced.reduced_simplices));

    const auto repeated = filtration_preserving_elementary_collapse(complex);
    REQUIRE_EQ(repeated.reduced_simplices, reduced.reduced_simplices);
    REQUIRE_EQ(repeated.collapses, reduced.collapses);
  }
  REQUIRE(total_collapses > 100);
}
