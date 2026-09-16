#pragma once
#include "algorithms/combinatorial/symmetric_submodular_minimization.hpp"
#include "test_framework.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>
namespace {
using algorithms::combinatorial::SymmetricSubmodularMinimizationResult;
using algorithms::combinatorial::SymmetricSubmodularOracle;
using algorithms::combinatorial::minimize_symmetric_submodular_function;
struct RecoveryHyperedge { std::uint64_t mask{}; std::int64_t weight{}; };
std::int64_t recovery_hypergraph_cut(std::size_t n,const std::vector<RecoveryHyperedge>& edges,const std::vector<std::size_t>& subset){
 std::uint64_t mask=0U; for(std::size_t v:subset) mask|=(std::uint64_t{1}<<v); const std::uint64_t all=(std::uint64_t{1}<<n)-1U; std::int64_t total=0;
 for(const auto&e:edges){const auto in=e.mask&mask;const auto out=e.mask&(all^mask);if(in!=0U&&out!=0U)total+=e.weight;} return total;}
std::int64_t recovery_exhaustive_min(std::size_t n,const SymmetricSubmodularOracle& oracle){std::int64_t best=std::numeric_limits<std::int64_t>::max();const auto lim=std::uint64_t{1}<<n;for(std::uint64_t m=1U;m+1U<lim;++m){std::vector<std::size_t>s;for(std::size_t v=0;v<n;++v)if((m&(std::uint64_t{1}<<v))!=0U)s.push_back(v);best=std::min(best,oracle(s));}return best;}
void recovery_require_result(std::size_t n,const SymmetricSubmodularOracle& oracle,const SymmetricSubmodularMinimizationResult&r){REQUIRE(!r.subset.empty());REQUIRE(r.subset.size()<n);REQUIRE(std::is_sorted(r.subset.begin(),r.subset.end()));REQUIRE_EQ(oracle(r.subset),r.value);REQUIRE_EQ(r.phases,n-1U);REQUIRE(r.oracle_calls>0U);REQUIRE(r.key_evaluations>0U);}
}
TEST_CASE(queyranne_validates_nontrivial_oracle_contract){SymmetricSubmodularOracle empty;REQUIRE_THROWS_AS(minimize_symmetric_submodular_function(0U,empty),std::invalid_argument);REQUIRE_THROWS_AS(minimize_symmetric_submodular_function(1U,[](const std::vector<std::size_t>&){return std::int64_t{0};}),std::invalid_argument);REQUIRE_THROWS_AS(minimize_symmetric_submodular_function(2U,empty),std::invalid_argument);}
TEST_CASE(queyranne_handles_full_width_exact_difference){const SymmetricSubmodularOracle oracle=[](const std::vector<std::size_t>&s){return s.size()==1U?std::numeric_limits<std::int64_t>::max():std::numeric_limits<std::int64_t>::min();};const auto r=minimize_symmetric_submodular_function(2U,oracle);REQUIRE_EQ(r.value,std::numeric_limits<std::int64_t>::max());recovery_require_result(2U,oracle,r);}
TEST_CASE(queyranne_minimizes_symmetric_cardinality_function){constexpr std::size_t n=7U;const SymmetricSubmodularOracle oracle=[](const std::vector<std::size_t>&s){constexpr std::size_t total=7U;return static_cast<std::int64_t>(std::min(s.size(),total-s.size()));};const auto first=minimize_symmetric_submodular_function(n,oracle);const auto second=minimize_symmetric_submodular_function(n,oracle);REQUIRE(first==second);REQUIRE_EQ(first.value,1);recovery_require_result(n,oracle,first);}
TEST_CASE(queyranne_hypergraph_cut_matches_exhaustive_oracle){std::mt19937_64 rng(0x5155455952414E4EULL);for(std::size_t trial=0;trial<500U;++trial){const std::size_t n=2U+static_cast<std::size_t>(rng()%7U);const std::size_t ec=static_cast<std::size_t>(rng()%14U);const auto all=(std::uint64_t{1}<<n)-1U;std::vector<RecoveryHyperedge>edges;for(std::size_t i=0;i<ec;++i){std::uint64_t mask=rng()&all;if(mask==0U)mask=1U;edges.push_back({mask,static_cast<std::int64_t>(rng()%21U)});}const SymmetricSubmodularOracle oracle=[&,n](const std::vector<std::size_t>&s){return recovery_hypergraph_cut(n,edges,s);};const auto r=minimize_symmetric_submodular_function(n,oracle);const auto repeated=minimize_symmetric_submodular_function(n,oracle);REQUIRE(r==repeated);recovery_require_result(n,oracle,r);REQUIRE_EQ(r.value,recovery_exhaustive_min(n,oracle));}}
