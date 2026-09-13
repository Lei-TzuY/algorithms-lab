#include "test_framework.hpp"
#include "test_distinct_degree_factorization_cases.hpp"
#include "test_equal_degree_factorization_cases.hpp"
#include "test_irreducible_factorization_cases.hpp"
#include "test_spanning_tree_count_cases.hpp"
#include "test_earley_parser_cases.hpp"
#include "test_earley_parser_randomized_cases.hpp"
#include "test_extension_field_cases.hpp"
#include "test_bitwise_convolution_cases.hpp"
#include "test_metric_tsp_cases.hpp"
#include "test_euler_tour_forest_cases.hpp"
#include "test_fully_dynamic_connectivity_cases.hpp"
#include "test_dynamic_minimum_spanning_forest_cases.hpp"
#include "test_dulmage_mendelsohn_cases.hpp"
#include "test_lz77_cases.hpp"
#include "test_bareiss_cases.hpp"
#include "test_reed_solomon_cases.hpp"
#include "test_vertex_connectivity_cases.hpp"
#include "test_radix_heap_cases.hpp"
#include "test_elias_fano_cases.hpp"
#include "test_smith_normal_form_cases.hpp"
#include "test_modular_linear_system_cases.hpp"
#include "test_weighted_matroid_intersection_cases.hpp"
#include "test_matroid_union_cases.hpp"
#include "test_k_shortest_paths_cases.hpp"
#include "test_fractional_cascading_cases.hpp"
#include "test_optimal_bst_cases.hpp"
#include "test_general_graph_isomorphism_cases.hpp"
#include "test_cartesian_tree_rmq_cases.hpp"
#include "test_dsu_on_tree_frequency_cases.hpp"
#include "test_cactus_decomposition_cases.hpp"
#include "test_parity_game_cases.hpp"
#include "test_fft_cases.hpp"
#include "test_strassen_matrix_cases.hpp"
#include "test_push_relabel_cases.hpp"
#include "test_lyndon_factorization_cases.hpp"
#include "test_delaunay_triangulation_cases.hpp"
#include "test_minkowski_sum_cases.hpp"
#include "test_multipoint_evaluation_cases.hpp"
#include "test_prime_counting_cases.hpp"
#include "test_weighted_set_cover_cases.hpp"
#include "test_conjugate_gradient_cases.hpp"
#include "test_cholesky_cases.hpp"
#include "test_lu_factorization_cases.hpp"
#include "test_householder_qr_cases.hpp"
#include "test_bidiagonalization_cases.hpp"
#include "test_svd_cases.hpp"
#include "test_minimum_enclosing_circle_cases.hpp"
#include "test_strong_orientation_cases.hpp"
#include "test_open_ear_decomposition_cases.hpp"
#include "test_st_numbering_cases.hpp"
#include "test_densest_subgraph_cases.hpp"
#include "test_feedback_vertex_set_cases.hpp"
#include "test_tutte_polynomial_cases.hpp"
#include "test_minimum_dominating_set_cases.hpp"
#include "test_binary_permanent_cases.hpp"
#include "test_linear_extension_count_cases.hpp"
#include "test_gallai_edmonds_cases.hpp"
#include "test_tutte_berge_cases.hpp"
#include "test_cuckoo_hash_set_cases.hpp"
#include "test_burnside_orbit_count_cases.hpp"
#include "test_manacher_cases.hpp"
#include "test_prufer_cases.hpp"
#include "test_scapegoat_tree_cases.hpp"
#include "test_skip_list_cases.hpp"
#include "test_x_fast_trie_cases.hpp"
#include "test_sprague_grundy_cases.hpp"
#include "test_a_star_cases.hpp"
#include "test_greedy_spanner_cases.hpp"

int main() {
  std::size_t passed = 0;
  for (const auto& test : testfw::registry()) {
    try {
      test.function();
      ++passed;
      std::cout << "[PASS] " << test.name << '\n';
    } catch (const std::exception& error) {
      std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
      return 1;
    } catch (...) {
      std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
      return 1;
    }
  }
  std::cout << passed << " tests passed\n";
  return 0;
}
