#pragma once

#include "algorithms/topology/persistent_homology.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using algorithms::topology::FilteredSimplex;
using algorithms::topology::PersistenceInterval;
using algorithms::topology::persistent_homology_z2;

std::size_t gf2_rank(std::vector<std::vector<unsigned char>> matrix) {
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
    std::swap(matrix[rank], matrix[pivot]);
    for (std::size_t row = 0; row < rows; ++row) {
      if (row != rank && matrix[row][column] != 0) {
        for (std::size_t c = column; c < columns; ++c) {
          matrix[row][c] ^= matrix[rank][c];
        }
      }
    }
    ++rank;
  }
  return rank;
}

std::vector<std::size_t> prefix_betti(
    const std::vector<FilteredSimplex>& ordered, std::int64_t threshold) {
  std::vector<std::size_t> active_indices;
  std::size_t max_dimension = 0;
  for (std::size_t index = 0; index < ordered.size(); ++index) {
    if (ordered[index].filtration <= threshold) {
      active_indices.push_back(index);
      max_dimension = std::max(max_dimension, ordered[index].vertices.size() - 1);
    }
  }
  if (active_indices.empty()) {
    return {};
  }

  std::map<std::vector<std::size_t>, std::size_t> active_by_vertices;
  std::vector<std::vector<std::size_t>> by_dimension(max_dimension + 1);
  for (const std::size_t index : active_indices) {
    active_by_vertices.emplace(ordered[index].vertices, index);
    by_dimension[ordered[index].vertices.size() - 1].push_back(index);
  }

  std::vector<std::size_t> ranks(max_dimension + 2, 0);
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
      const auto& simplex = ordered[domain[column]].vertices;
      for (std::size_t removed = 0; removed < simplex.size(); ++removed) {
        std::vector<std::size_t> face;
        face.reserve(simplex.size() - 1);
        for (std::size_t position = 0; position < simplex.size(); ++position) {
          if (position != removed) {
            face.push_back(simplex[position]);
          }
        }
        const auto face_index = active_by_vertices.find(face);
        REQUIRE(face_index != active_by_vertices.end());
        matrix[row_of.at(face_index->second)][column] ^= 1;
      }
    }
    ranks[dimension] = gf2_rank(std::move(matrix));
  }

  std::vector<std::size_t> betti(max_dimension + 1, 0);
  for (std::size_t dimension = 0; dimension <= max_dimension; ++dimension) {
    betti[dimension] = by_dimension[dimension].size() - ranks[dimension] -
                       ranks[dimension + 1];
  }
  return betti;
}

std::vector<std::size_t> barcode_betti(
    std::span<const PersistenceInterval> intervals, std::int64_t threshold) {
  std::size_t max_dimension = 0;
  for (const auto& interval : intervals) {
    max_dimension = std::max(max_dimension, interval.dimension);
  }
  std::vector<std::size_t> counts(intervals.empty() ? 0 : max_dimension + 1, 0);
  for (const auto& interval : intervals) {
    if (interval.birth <= threshold &&
        (!interval.death.has_value() || threshold < *interval.death)) {
      ++counts[interval.dimension];
    }
  }
  return counts;
}

void require_same_betti(std::vector<std::size_t> left,
                        std::vector<std::size_t> right) {
  const std::size_t size = std::max(left.size(), right.size());
  left.resize(size, 0);
  right.resize(size, 0);
  REQUIRE_EQ(left, right);
}

TEST_CASE(persistent_homology_validation) {
  REQUIRE(persistent_homology_z2({}).intervals.empty());
  REQUIRE_THROWS_AS(
      persistent_homology_z2(std::vector<FilteredSimplex>{{{}, 0}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      persistent_homology_z2(
          std::vector<FilteredSimplex>{{{0, 0}, 0}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      persistent_homology_z2(
          std::vector<FilteredSimplex>{{{0}, 0}, {{0}, 1}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      persistent_homology_z2(
          std::vector<FilteredSimplex>{{{0}, 0}, {{1}, 0}, {{0, 1}, -1}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      persistent_homology_z2(
          std::vector<FilteredSimplex>{{{0}, 0}, {{0, 1}, 1}}),
      std::invalid_argument);
}

TEST_CASE(persistent_homology_filled_triangle_barcode) {
  const std::vector<FilteredSimplex> complex{
      {{0}, 0}, {{1}, 0}, {{2}, 0},
      {{0, 1}, 1}, {{1, 2}, 1}, {{0, 2}, 1},
      {{0, 1, 2}, 2}};
  const auto result = persistent_homology_z2(complex);
  REQUIRE_EQ(result.intervals.size(), std::size_t{4});

  std::size_t h0_essential = 0;
  std::size_t h0_dying_at_one = 0;
  std::size_t h1_dying_at_two = 0;
  for (const auto& interval : result.intervals) {
    if (interval.dimension == 0 && !interval.death.has_value()) {
      ++h0_essential;
    }
    if (interval.dimension == 0 && interval.death == std::optional<std::int64_t>{1}) {
      ++h0_dying_at_one;
    }
    if (interval.dimension == 1 && interval.birth == 1 &&
        interval.death == std::optional<std::int64_t>{2}) {
      ++h1_dying_at_two;
    }
  }
  REQUIRE_EQ(h0_essential, std::size_t{1});
  REQUIRE_EQ(h0_dying_at_one, std::size_t{2});
  REQUIRE_EQ(h1_dying_at_two, std::size_t{1});
  require_same_betti(barcode_betti(result.intervals, 0),
                     std::vector<std::size_t>{3, 0});
  require_same_betti(barcode_betti(result.intervals, 1),
                     std::vector<std::size_t>{1, 1});
  require_same_betti(barcode_betti(result.intervals, 2),
                     std::vector<std::size_t>{1, 0});
}

TEST_CASE(persistent_homology_tetrahedron_boundary_has_h2) {
  std::vector<FilteredSimplex> complex;
  for (std::size_t vertex = 0; vertex < 4; ++vertex) {
    complex.push_back({{vertex}, 0});
  }
  for (std::size_t a = 0; a < 4; ++a) {
    for (std::size_t b = a + 1; b < 4; ++b) {
      complex.push_back({{a, b}, 1});
    }
  }
  complex.push_back({{0, 1, 2}, 2});
  complex.push_back({{0, 1, 3}, 2});
  complex.push_back({{0, 2, 3}, 2});
  complex.push_back({{1, 2, 3}, 2});
  const auto result = persistent_homology_z2(complex);
  require_same_betti(barcode_betti(result.intervals, 2),
                     std::vector<std::size_t>{1, 0, 1});
  const auto repeated = persistent_homology_z2(complex);
  REQUIRE_EQ(repeated.ordered_simplices, result.ordered_simplices);
  REQUIRE_EQ(repeated.intervals, result.intervals);
}

TEST_CASE(persistent_homology_randomized_prefix_betti_differential) {
  std::mt19937_64 random(0xA11CEBADC0DEULL);
  std::uniform_int_distribution<int> vertex_count_distribution(0, 6);
  std::uniform_int_distribution<int> filtration_distribution(0, 2);
  std::bernoulli_distribution edge_inclusion(0.55);
  std::bernoulli_distribution triangle_inclusion(0.35);

  for (int trial = 0; trial < 700; ++trial) {
    const std::size_t n = static_cast<std::size_t>(vertex_count_distribution(random));
    std::vector<FilteredSimplex> complex;
    std::vector<std::int64_t> vertex_filtration(n, 0);
    for (std::size_t vertex = 0; vertex < n; ++vertex) {
      vertex_filtration[vertex] = filtration_distribution(random);
      complex.push_back({{vertex}, vertex_filtration[vertex]});
    }

    std::map<std::pair<std::size_t, std::size_t>, std::int64_t> edge_filtration;
    for (std::size_t left = 0; left < n; ++left) {
      for (std::size_t right = left + 1; right < n; ++right) {
        if (!edge_inclusion(random)) {
          continue;
        }
        const std::int64_t filtration =
            std::max(vertex_filtration[left], vertex_filtration[right]) +
            filtration_distribution(random);
        edge_filtration.emplace(std::pair{left, right}, filtration);
        complex.push_back({{left, right}, filtration});
      }
    }

    for (std::size_t a = 0; a < n; ++a) {
      for (std::size_t b = a + 1; b < n; ++b) {
        for (std::size_t c = b + 1; c < n; ++c) {
          const auto ab = edge_filtration.find({a, b});
          const auto ac = edge_filtration.find({a, c});
          const auto bc = edge_filtration.find({b, c});
          if (ab == edge_filtration.end() || ac == edge_filtration.end() ||
              bc == edge_filtration.end() || !triangle_inclusion(random)) {
            continue;
          }
          const std::int64_t filtration =
              std::max({ab->second, ac->second, bc->second}) +
              filtration_distribution(random);
          complex.push_back({{a, b, c}, filtration});
        }
      }
    }

    std::shuffle(complex.begin(), complex.end(), random);
    const auto result = persistent_homology_z2(complex);
    std::set<std::int64_t> thresholds;
    for (const auto& simplex : result.ordered_simplices) {
      thresholds.insert(simplex.filtration);
    }
    for (const std::int64_t threshold : thresholds) {
      require_same_betti(prefix_betti(result.ordered_simplices, threshold),
                         barcode_betti(result.intervals, threshold));
    }
    const auto repeated = persistent_homology_z2(complex);
    REQUIRE_EQ(repeated.ordered_simplices, result.ordered_simplices);
    REQUIRE_EQ(repeated.intervals, result.intervals);
  }
}

}  // namespace
