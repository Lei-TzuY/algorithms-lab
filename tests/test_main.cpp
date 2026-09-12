#include "test_framework.hpp"
#include "test_distinct_degree_factorization_cases.hpp"
#include "test_equal_degree_factorization_cases.hpp"
#include "test_irreducible_factorization_cases.hpp"
#include "test_spanning_tree_count_cases.hpp"
#include "test_earley_parser_cases.hpp"
#include "test_earley_parser_randomized_cases.hpp"
#include "test_extension_field_cases.hpp"
#include "test_metric_tsp_cases.hpp"
#include "test_euler_tour_forest_cases.hpp"
#include "test_lz77_cases.hpp"
#include "test_bareiss_cases.hpp"
#include "test_reed_solomon_cases.hpp"
#include "test_vertex_connectivity_cases.hpp"

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
