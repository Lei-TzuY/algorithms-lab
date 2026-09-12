#include "algorithms/graphs/randomized_min_cut.hpp"
#include "algorithms/randomized/freivalds.hpp"

#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using algorithms::graphs::Graph;
using algorithms::graphs::KargerMinCutResult;
using algorithms::graphs::Vertex;
using algorithms::graphs::karger_randomized_global_min_cut;
using algorithms::randomized::ModularMatrix;
using algorithms::randomized::freivalds_randomized_verify_matrix_product;

std::vector<std::pair<Vertex, Vertex>> edges_of(const Graph& graph) {
  std::vector<std::pair<Vertex, Vertex>> edges;
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (from < edge.to) {
        edges.emplace_back(from, edge.to);
      }
    }
  }
  return edges;
}

std::size_t replay_cut(const Graph& graph, const std::vector<bool>& side) {
  REQUIRE_EQ(side.size(), graph.vertex_count());
  std::size_t cut = 0;
  for (const auto& [from, to] : edges_of(graph)) {
    if (side[from] != side[to]) {
      ++cut;
    }
  }
  return cut;
}

std::size_t exhaustive_min_cut(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  if (n <= 1) {
    return 0;
  }
  const auto edges = edges_of(graph);
  const std::uint64_t assignments = std::uint64_t{1} << (n - 1);
  std::size_t best = std::numeric_limits<std::size_t>::max();
  for (std::uint64_t mask = 0; mask + 1 < assignments; ++mask) {
    std::vector<bool> side(n, false);
    side[0] = true;
    for (Vertex vertex = 1; vertex < n; ++vertex) {
      side[vertex] = ((mask >> (vertex - 1)) & 1U) != 0;
    }
    std::size_t cut = 0;
    for (const auto& [from, to] : edges) {
      if (side[from] != side[to]) {
        ++cut;
      }
    }
    if (cut < best) {
      best = cut;
    }
  }
  return best;
}

ModularMatrix small_exact_product(const ModularMatrix& first,
                                  const ModularMatrix& second,
                                  const std::uint64_t modulus) {
  ModularMatrix result(first.size(),
                       std::vector<std::uint64_t>(second.front().size(), 0U));
  for (std::size_t row = 0U; row < first.size(); ++row) {
    for (std::size_t inner = 0U; inner < second.size(); ++inner) {
      for (std::size_t column = 0U; column < second.front().size(); ++column) {
        // This test-only oracle is called only with modulus 101. Ordinary
        // multiplication/addition therefore stays far below uint64_t overflow.
        result[row][column] =
            (result[row][column] + first[row][inner] * second[inner][column]) %
            modulus;
      }
    }
  }
  return result;
}

void verify_witness(const Graph& graph, const KargerMinCutResult& result) {
  REQUIRE_EQ(result.side.size(), graph.vertex_count());
  REQUIRE_EQ(replay_cut(graph, result.side), result.best_cut_size);
  if (graph.vertex_count() >= 2) {
    bool has_left = false;
    bool has_right = false;
    for (bool side : result.side) {
      has_left = has_left || side;
      has_right = has_right || !side;
    }
    REQUIRE(has_left);
    REQUIRE(has_right);
  }
}

TEST_CASE(karger_contract_validation_and_trivial_graphs) {
  Graph directed(2, true);
  directed.add_edge(0, 1);
  REQUIRE_THROWS_AS(karger_randomized_global_min_cut(directed, 1, 1), std::invalid_argument);

  Graph empty(0, false);
  REQUIRE_THROWS_AS(karger_randomized_global_min_cut(empty, 1, 0), std::invalid_argument);
  const auto empty_result = karger_randomized_global_min_cut(empty, 7, 3);
  REQUIRE_EQ(empty_result.best_cut_size, std::size_t{0});
  REQUIRE_EQ(empty_result.trials_executed, std::size_t{0});
  REQUIRE(!empty_result.winning_trial.has_value());

  Graph singleton(1, false);
  singleton.add_edge(0, 0);
  const auto singleton_result = karger_randomized_global_min_cut(singleton, 9, 4);
  REQUIRE_EQ(singleton_result.best_cut_size, std::size_t{0});
  REQUIRE_EQ(singleton_result.trials_executed, std::size_t{0});
}

TEST_CASE(karger_disconnected_graph_is_exact_zero_without_trials) {
  Graph graph(5, false);
  graph.add_edge(0, 1);
  graph.add_edge(1, 2);
  graph.add_edge(3, 4);
  const auto result = karger_randomized_global_min_cut(graph, 123, 50);
  REQUIRE_EQ(result.best_cut_size, std::size_t{0});
  REQUIRE_EQ(result.requested_trials, std::size_t{50});
  REQUIRE_EQ(result.trials_executed, std::size_t{0});
  REQUIRE(!result.winning_trial.has_value());
  verify_witness(graph, result);
}

TEST_CASE(karger_parallel_edges_count_and_weights_do_not) {
  Graph graph(2, false);
  graph.add_edge(0, 1, 100);
  graph.add_edge(0, 1, -7);
  graph.add_edge(0, 1, 0);
  graph.add_edge(0, 0, 999);
  const auto result = karger_randomized_global_min_cut(graph, 42, 5);
  REQUIRE_EQ(result.best_cut_size, std::size_t{3});
  REQUIRE_EQ(result.trials_executed, std::size_t{5});
  REQUIRE_EQ(result.winning_trial.value(), std::size_t{0});
  verify_witness(graph, result);
}

TEST_CASE(karger_fixed_seed_is_replayable) {
  Graph graph(6, false);
  graph.add_edge(0, 1);
  graph.add_edge(1, 2);
  graph.add_edge(2, 0);
  graph.add_edge(2, 3);
  graph.add_edge(3, 4);
  graph.add_edge(4, 5);
  graph.add_edge(5, 3);
  graph.add_edge(1, 4);

  const auto first = karger_randomized_global_min_cut(graph, 0xC0FFEEULL, 32);
  const auto second = karger_randomized_global_min_cut(graph, 0xC0FFEEULL, 32);
  REQUIRE_EQ(first.best_cut_size, second.best_cut_size);
  REQUIRE_EQ(first.side, second.side);
  REQUIRE_EQ(first.winning_trial, second.winning_trial);
  REQUIRE_EQ(first.seed, second.seed);
  verify_witness(graph, first);
}

TEST_CASE(karger_one_trial_can_be_suboptimal_and_more_trials_can_improve) {
  Graph graph(6, false);
  graph.add_edge(0, 4);
  graph.add_edge(0, 5);
  graph.add_edge(1, 2);
  graph.add_edge(1, 4);
  graph.add_edge(1, 5);
  graph.add_edge(2, 3);
  graph.add_edge(2, 4);
  graph.add_edge(2, 5);
  graph.add_edge(3, 4);
  graph.add_edge(3, 5);
  graph.add_edge(4, 5);

  REQUIRE_EQ(exhaustive_min_cut(graph), std::size_t{2});
  const auto one_trial = karger_randomized_global_min_cut(graph, 1, 1);
  REQUIRE_EQ(one_trial.best_cut_size, std::size_t{3});
  REQUIRE_EQ(one_trial.trials_executed, std::size_t{1});
  verify_witness(graph, one_trial);

  const auto three_trials = karger_randomized_global_min_cut(graph, 1, 3);
  REQUIRE_EQ(three_trials.best_cut_size, std::size_t{2});
  REQUIRE_EQ(three_trials.winning_trial.value(), std::size_t{2});
  verify_witness(graph, three_trials);
}

TEST_CASE(karger_randomized_small_graphs_match_exhaustive_with_fixed_budget) {
  std::mt19937_64 rng(0x4B4152474552ULL);
  std::uniform_int_distribution<int> n_distribution(2, 8);
  std::bernoulli_distribution edge_distribution(0.38);
  std::bernoulli_distribution parallel_distribution(0.12);

  for (std::size_t case_index = 0; case_index < 240; ++case_index) {
    const std::size_t n = static_cast<std::size_t>(n_distribution(rng));
    Graph graph(n, false);
    for (Vertex from = 0; from < n; ++from) {
      if ((case_index + from) % 11 == 0) {
        graph.add_edge(from, from);
      }
      for (Vertex to = from + 1; to < n; ++to) {
        if (!edge_distribution(rng)) {
          continue;
        }
        graph.add_edge(from, to, static_cast<std::int64_t>(from + to + 1));
        if (parallel_distribution(rng)) {
          graph.add_edge(from, to, -static_cast<std::int64_t>(to + 1));
        }
      }
    }

    const auto result = karger_randomized_global_min_cut(
        graph, 0x9E3779B97F4A7C15ULL ^ case_index, 512);
    verify_witness(graph, result);
    REQUIRE_EQ(result.best_cut_size, exhaustive_min_cut(graph));
  }
}

TEST_CASE(freivalds_validates_shapes_field_trials_and_residues) {
  const ModularMatrix one{{1U}};
  REQUIRE_THROWS_AS(freivalds_randomized_verify_matrix_product(
                        one, one, one, 2U, 0U, 0U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(freivalds_randomized_verify_matrix_product(
                        one, one, one, 4U, 0U, 1U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(freivalds_randomized_verify_matrix_product(
                        {}, one, one, 2U, 0U, 1U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(freivalds_randomized_verify_matrix_product(
                        {{1U, 0U}}, one, one, 2U, 0U, 1U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(freivalds_randomized_verify_matrix_product(
                        {{1U}, {0U, 1U}}, one, one, 2U, 0U, 1U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(freivalds_randomized_verify_matrix_product(
                        {{2U}}, one, one, 2U, 0U, 1U),
                    std::invalid_argument);
}

TEST_CASE(freivalds_equal_product_is_one_sided_and_replayable) {
  const ModularMatrix first{{2U, 3U, 5U}, {7U, 11U, 13U}};
  const ModularMatrix second{{17U, 19U}, {23U, 29U}, {31U, 37U}};
  const auto product = small_exact_product(first, second, 101U);
  const auto first_run = freivalds_randomized_verify_matrix_product(
      first, second, product, 101U, 0xF3E1A1D5ULL, 32U);
  const auto replay = freivalds_randomized_verify_matrix_product(
      first, second, product, 101U, 0xF3E1A1D5ULL, 32U);
  REQUIRE(first_run.accepted);
  REQUIRE_EQ(first_run, replay);
  REQUIRE_EQ(first_run.trials_executed, std::size_t{32});
  REQUIRE(!first_run.rejecting_trial.has_value());
}

TEST_CASE(freivalds_finite_randomized_acceptance_is_not_exact_equality) {
  const ModularMatrix first{{1U, 0U}};
  const ModularMatrix second{{1U, 0U}, {0U, 1U}};
  const ModularMatrix wrong{{0U, 0U}};

  const auto false_accept = freivalds_randomized_verify_matrix_product(
      first, second, wrong, 2U, 0U, 1U);
  REQUIRE(false_accept.accepted);
  REQUIRE_EQ(false_accept.trials_executed, std::size_t{1});

  const auto rejection = freivalds_randomized_verify_matrix_product(
      first, second, wrong, 2U, 3U, 1U);
  REQUIRE(!rejection.accepted);
  REQUIRE_EQ(rejection.rejecting_trial.value(), std::size_t{0});
  REQUIRE_EQ(rejection.trials_executed, std::size_t{1});
}

TEST_CASE(freivalds_full_uint64_prime_known_answer_product) {
  constexpr std::uint64_t modulus = 18446744073709551557ULL;
  const ModularMatrix first{{modulus - 2U, modulus - 3U},
                            {modulus - 5U, 1234567890123456789ULL}};
  const ModularMatrix second{{modulus - 7U, 998244353U},
                             {42U, modulus - 11U}};
  const ModularMatrix expected{{18446744073709551445ULL,
                                18446744071713062884ULL},
                               {14958363237766082059ULL,
                                4866497277360305113ULL}};
  const auto result = freivalds_randomized_verify_matrix_product(
      first, second, expected, modulus, 0xFFFFFFFF00000001ULL, 16U);
  REQUIRE(result.accepted);
}

TEST_CASE(freivalds_random_small_products_and_corruptions) {
  constexpr std::uint64_t modulus = 101U;
  std::mt19937_64 random(0x4652454956414C44ULL);
  for (std::size_t trial = 0U; trial < 500U; ++trial) {
    const std::size_t rows = 1U + static_cast<std::size_t>(random() % 5U);
    const std::size_t shared = 1U + static_cast<std::size_t>(random() % 5U);
    const std::size_t columns = 1U + static_cast<std::size_t>(random() % 5U);
    ModularMatrix first(rows, std::vector<std::uint64_t>(shared));
    ModularMatrix second(shared, std::vector<std::uint64_t>(columns));
    for (auto& row : first) {
      for (auto& value : row) {
        value = random() % modulus;
      }
    }
    for (auto& row : second) {
      for (auto& value : row) {
        value = random() % modulus;
      }
    }
    const auto exact = small_exact_product(first, second, modulus);
    REQUIRE(freivalds_randomized_verify_matrix_product(
                first, second, exact, modulus,
                trial ^ 0x9E3779B97F4A7C15ULL, 8U)
                .accepted);

    auto corrupted = exact;
    const std::size_t corrupt_row = static_cast<std::size_t>(random() % rows);
    const std::size_t corrupt_column =
        static_cast<std::size_t>(random() % columns);
    corrupted[corrupt_row][corrupt_column] =
        (corrupted[corrupt_row][corrupt_column] + 1U) % modulus;
    const auto result = freivalds_randomized_verify_matrix_product(
        first, second, corrupted, modulus,
        trial ^ 0xD1B54A32D192ED03ULL, 8U);
    REQUIRE(!result.accepted);
    REQUIRE(result.rejecting_trial.has_value());
  }
}

}  // namespace
