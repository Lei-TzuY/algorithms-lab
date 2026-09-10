cmake_minimum_required(VERSION 3.20)
project(algorithms_lab VERSION 0.4.0 LANGUAGES CXX)

option(ALGORITHMS_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(ALGORITHMS_BUILD_BENCHMARKS "Build the reproducible benchmark harness" ON)
option(ALGORITHMS_BUILD_EXAMPLES "Build examples" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(algorithms_enable_strict_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  else()
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror)
  endif()
endfunction()

function(algorithms_enable_sanitizers target)
  if(ALGORITHMS_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  endif()
endfunction()

add_library(algorithms_lab
  src/approximation/vertex_cover.cpp
  src/data_structures/byte_wavelet_matrix.cpp
  src/data_structures/packed_rank_select.cpp
  src/data_structures/run_length_byte_rank.cpp
  src/data_structures/persistent_segment_tree.cpp
  src/data_structures/splay_set.cpp
  src/dynamic_programming/edit_distance.cpp
  src/dynamic_programming/knapsack.cpp
  src/dynamic_programming/longest_increasing_subsequence.cpp
  src/dynamic_programming/matrix_chain.cpp
  src/dynamic_programming/tree_independent_set.cpp
  src/geometry/geometry2d.cpp
  src/greedy/interval_scheduling.cpp
  src/graphs/graph.cpp
  src/graphs/directed_arborescence.cpp
  src/graphs/dominator_tree.cpp
  src/graphs/dominance_frontier.cpp
  src/graphs/ssa_construction.cpp
  src/graphs/ssa_scalar_semantics.cpp
  src/graphs/ssa_pruned.cpp
  src/graphs/ssa_destruction.cpp
  src/graphs/register_allocation.cpp
  src/graphs/register_coalescing.cpp
  src/graphs/backend_spill_lowering.cpp
  src/graphs/backend_register_reservation.cpp
  src/graphs/backend_frame_layout.cpp
  src/graphs/backend_frame_addressing.cpp
  src/graphs/backend_frame_base_reservation.cpp
  src/graphs/backend_frame_entry_setup.cpp
  src/graphs/backend_stack_pointer_reservation.cpp
  src/graphs/backend_frame_actions.cpp
  src/graphs/backend_frame_action_legalization.cpp
  src/graphs/backend_symbolic_frame_instructions.cpp
  src/graphs/backend_frame_bytecode.cpp
  src/graphs/backend_frame_bytecode_execution.cpp
  src/graphs/backend_spill_transfer_execution.cpp
  src/graphs/backend_instruction_continuation.cpp
  src/graphs/backend_cfg_path_continuation.cpp
  src/graphs/backend_cfg_path_exit_event.cpp
  src/graphs/traversal.cpp
  src/graphs/shortest_paths.cpp
  src/graphs/minimum_spanning_tree.cpp
  src/graphs/strongly_connected_components.cpp
  src/graphs/max_flow.cpp
  src/graphs/gomory_hu_tree.cpp
  src/graphs/min_cost_flow.cpp
  src/graphs/min_cost_circulation.cpp
  src/graphs/bipartite_matching.cpp
  src/graphs/general_matching.cpp
  src/graphs/randomized_min_cut.cpp
  src/graphs/lowest_common_ancestor.cpp
  src/graphs/heavy_light_decomposition.cpp
  src/graphs/link_cut_tree.cpp
  src/graphs/link_cut_tree_navigation.cpp
  src/graphs/offline_dynamic_connectivity.cpp
  src/online/paging.cpp
  src/optimization/hungarian_assignment.cpp
  src/number_theory/modular.cpp
  src/polynomials/number_theoretic_transform.cpp
  src/polynomials/formal_power_series.cpp
  src/streaming/misra_gries.cpp
  src/strings/bwt_index.cpp
  src/strings/bwt_text_extraction.cpp
  src/strings/bwt_seed_verify.cpp
  src/strings/bwt_candidate_local_seed_verify.cpp
  src/strings/bwt_coalesced_seed_verify.cpp
  src/strings/bwt_bidirectional.cpp
  src/strings/bwt_edit_distance.cpp
  src/strings/bwt_affine_gap.cpp
  src/strings/bwt_run_sampled_locate.cpp)

target_include_directories(algorithms_lab PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
algorithms_enable_strict_warnings(algorithms_lab)
algorithms_enable_sanitizers(algorithms_lab)

include(CTest)
if(BUILD_TESTING)
  add_executable(algorithms_tests
    tests/test_main.cpp
    tests/test_searching_sorting.cpp
    tests/test_data_structures.cpp
    tests/test_graphs.cpp
    tests/test_directed_arborescence.cpp
    tests/test_dominator_tree.cpp
    tests/test_dominance_frontier.cpp
    tests/test_ssa_construction.cpp
    tests/test_ssa_scalar_semantics.cpp
    tests/test_ssa_pruned.cpp
    tests/test_ssa_destruction.cpp
    tests/test_register_allocation.cpp
    tests/test_register_coalescing.cpp
    tests/test_backend_spill_lowering.cpp
    tests/test_backend_register_reservation.cpp
    tests/test_backend_frame_layout.cpp
    tests/test_backend_frame_addressing.cpp
    tests/test_backend_frame_base_reservation.cpp
    tests/test_backend_frame_entry_setup.cpp
    tests/test_backend_stack_pointer_reservation.cpp
    tests/test_backend_frame_actions.cpp
    tests/test_backend_frame_action_legalization.cpp
    tests/test_backend_symbolic_frame_instructions.cpp
    tests/test_backend_frame_bytecode.cpp
    tests/test_backend_frame_bytecode_execution.cpp
    tests/test_backend_spill_transfer_execution.cpp
    tests/test_backend_instruction_continuation.cpp
    tests/test_backend_cfg_path_continuation.cpp
    tests/test_backend_cfg_path_exit_event.cpp
    tests/test_shortest_paths.cpp
    tests/test_phase2.cpp
    tests/test_knapsack.cpp
    tests/test_lis.cpp
    tests/test_edit_distance.cpp
    tests/test_matrix_chain.cpp
    tests/test_tree_independent_set.cpp
    tests/test_fenwick_tree.cpp
    tests/test_segment_tree.cpp
    tests/test_sparse_table.cpp
    tests/test_byte_trie.cpp
    tests/test_rollback_dsu.cpp
    tests/test_splay_set.cpp
    tests/test_persistent_segment_tree.cpp
    tests/test_packed_rank_select.cpp
    tests/test_run_length_byte_rank.cpp
    tests/test_byte_wavelet_matrix.cpp
    tests/test_kmp.cpp
    tests/test_z_function.cpp
    tests/test_rolling_hash.cpp
    tests/test_suffix_array.cpp
    tests/test_bwt_index.cpp
    tests/test_bwt_text_extraction.cpp
    tests/test_bwt_seed_verify.cpp
    tests/test_bwt_candidate_local_seed_verify.cpp
    tests/test_bwt_coalesced_seed_verify.cpp
    tests/test_bwt_bidirectional.cpp
    tests/test_bwt_hamming.cpp
    tests/test_bwt_edit_distance.cpp
    tests/test_bwt_affine_gap.cpp
    tests/test_bwt_run_length_occurrence.cpp
    tests/test_bwt_toehold.cpp
    tests/test_bwt_run_sampled_locate.cpp
    tests/test_bwt_memoized_run_sampled_locate.cpp
    tests/test_max_flow.cpp
    tests/test_gomory_hu_tree.cpp
    tests/test_min_cost_flow.cpp
    tests/test_min_cost_circulation.cpp
    tests/test_hungarian_assignment.cpp
    tests/test_bipartite_matching.cpp
    tests/test_general_matching.cpp
    tests/test_randomized_min_cut.cpp
    tests/test_lowest_common_ancestor.cpp
    tests/test_heavy_light_decomposition.cpp
    tests/test_link_cut_tree.cpp
    tests/test_link_cut_tree_augmented.cpp
    tests/test_link_cut_tree_lazy_assignment.cpp
    tests/test_link_cut_tree_lazy_addition.cpp
    tests/test_link_cut_tree_navigation.cpp
    tests/test_link_cut_tree_rooted_subtree.cpp
    tests/test_link_cut_tree_rooted_subtree_sum.cpp
    tests/test_offline_dynamic_connectivity.cpp
    tests/test_online_paging.cpp
    tests/test_geometry2d.cpp
    tests/test_number_theory.cpp
    tests/test_number_theoretic_transform.cpp
    tests/test_exact_convolution.cpp
    tests/test_formal_power_series.cpp
    tests/test_vertex_cover_approximation.cpp
    tests/test_misra_gries.cpp)
  target_include_directories(algorithms_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_link_libraries(algorithms_tests PRIVATE algorithms_lab)
  algorithms_enable_strict_warnings(algorithms_tests)
  algorithms_enable_sanitizers(algorithms_tests)
  add_test(NAME algorithms_tests COMMAND algorithms_tests)
endif()

if(ALGORITHMS_BUILD_BENCHMARKS)
  add_executable(algorithms_benchmark benchmarks/benchmark_main.cpp)
  target_link_libraries(algorithms_benchmark PRIVATE algorithms_lab)
  algorithms_enable_strict_warnings(algorithms_benchmark)
  algorithms_enable_sanitizers(algorithms_benchmark)
endif()

if(ALGORITHMS_BUILD_EXAMPLES)
  add_executable(graph_search_demo examples/graph_search_demo.cpp)
  target_link_libraries(graph_search_demo PRIVATE algorithms_lab)
  algorithms_enable_strict_warnings(graph_search_demo)
  algorithms_enable_sanitizers(graph_search_demo)
endif()
