#pragma once
#include "algorithms/data_structures/kinetic_affine_tournament.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace kinetic_test_detail {
using algorithms::data_structures::AffineTrajectory;
using algorithms::data_structures::DiscreteKineticAffineTournament;
using algorithms::data_structures::KineticMinimumWitness;

std::optional<KineticMinimumWitness> scan(const std::vector<AffineTrajectory>& a, std::int64_t t) {
  if (a.empty()) return std::nullopt;
  std::size_t best=0;
  auto value=[&](std::size_t i){return a[i].slope*t+a[i].intercept;};
  for(std::size_t i=1;i<a.size();++i){ if(value(i)<value(best)||(value(i)==value(best)&&i<best)) best=i; }
  return KineticMinimumWitness{best,value(best)};
}
}

TEST_CASE(kinetic_affine_empty_and_validation) {
  using namespace kinetic_test_detail;
  DiscreteKineticAffineTournament empty(std::vector<AffineTrajectory>{}, 0);
  REQUIRE(!empty.minimum().has_value());
  REQUIRE(empty.valid_structure());
  REQUIRE_EQ(empty.advance_to(100), std::size_t{0});
  REQUIRE_THROWS_AS(DiscreteKineticAffineTournament(std::vector<AffineTrajectory>{{1'000'000'001LL,0}},0), std::out_of_range);
  REQUIRE_THROWS_AS(DiscreteKineticAffineTournament(std::vector<AffineTrajectory>{{0,0}},1'000'000'001LL), std::out_of_range);
  REQUIRE_THROWS_AS(empty.advance_to(-1), std::invalid_argument);
}

TEST_CASE(kinetic_affine_crossing_and_tie_break) {
  using namespace kinetic_test_detail;
  std::vector<AffineTrajectory> a{{0,0},{-1,3}};
  DiscreteKineticAffineTournament k(a,0);
  REQUIRE_EQ(k.minimum(), scan(a,0));
  REQUIRE_EQ(k.next_certificate_failure_time(), std::optional<std::int64_t>{4});
  REQUIRE_EQ(k.advance_to(3), std::size_t{0});
  REQUIRE_EQ(k.minimum(), scan(a,3)); // tie, id 0 wins
  REQUIRE_EQ(k.advance_to(4), std::size_t{1});
  REQUIRE_EQ(k.minimum(), scan(a,4));
  REQUIRE(k.valid_structure());

  std::vector<AffineTrajectory> b{{1,0},{0,0}};
  DiscreteKineticAffineTournament t(b,-1);
  REQUIRE_EQ(t.next_certificate_failure_time(), std::optional<std::int64_t>{1});
  REQUIRE_EQ(t.advance_to(0), std::size_t{0}); // tie keeps the smaller id 
  REQUIRE_EQ(t.minimum(), scan(b,0));
  REQUIRE_EQ(t.advance_to(1), std::size_t{1});
  REQUIRE_EQ(t.minimum(), scan(b,1));
}

TEST_CASE(kinetic_affine_simultaneous_failures_and_replay) {
  using namespace kinetic_test_detail;
  std::vector<AffineTrajectory> a{{0,0},{-1,2},{-2,4},{-3,6},{1,-10}};
  DiscreteKineticAffineTournament first(a,-5), second(a,-5);
  for(std::int64_t t=-5;t<=10;++t){
    first.advance_to(t); second.advance_to(t);
    REQUIRE_EQ(first.minimum(),scan(a,t));
    REQUIRE_EQ(second.minimum(),scan(a,t));
    REQUIRE_EQ(first.processed_certificate_failures(),second.processed_certificate_failures());
    REQUIRE(first.valid_structure()); REQUIRE(second.valid_structure());
  }
}

TEST_CASE(kinetic_affine_exact_public_boundaries) {
  using namespace kinetic_test_detail;
  std::vector<AffineTrajectory> a{{1'000'000'000LL,1'000'000'000LL},{-1'000'000'000LL,-1'000'000'000LL}};
  DiscreteKineticAffineTournament k(a,-1'000'000'000LL);
  REQUIRE_EQ(k.minimum(),scan(a,-1'000'000'000LL));
  k.advance_to(1'000'000'000LL);
  REQUIRE_EQ(k.minimum(),scan(a,1'000'000'000LL));
  REQUIRE(k.valid_structure());
}

TEST_CASE(kinetic_affine_randomized_full_scan_differential) {
  using namespace kinetic_test_detail;
  std::mt19937_64 rng(0x4B494E45544943ULL);
  std::uniform_int_distribution<int> n_dist(0,32), coef(-100,100), start_dist(-200,0), step_dist(0,8);
  for(int trial=0;trial<500;++trial){
    std::vector<AffineTrajectory> a(static_cast<std::size_t>(n_dist(rng)));
    for(auto& x:a){x.slope=coef(rng);x.intercept=coef(rng);}
    std::int64_t t=start_dist(rng);
    DiscreteKineticAffineTournament k(a,t);
    for(int step=0;step<180;++step){
      t=std::min<std::int64_t>(1000,t+step_dist(rng));
      k.advance_to(t);
      REQUIRE_EQ(k.minimum(),scan(a,t));
      REQUIRE(k.valid_structure());
    }
  }
}

TEST_CASE(kinetic_affine_monotone_time_and_certificate_diagnostics) {
  using namespace kinetic_test_detail;
  std::vector<AffineTrajectory> a{{2,5},{0,2},{-1,20},{3,-4}};
  DiscreteKineticAffineTournament k(a,-10);
  std::size_t last_count=0;
  for(std::int64_t t=-10;t<=30;++t){
    const auto next=k.next_certificate_failure_time();
    if(next) REQUIRE(*next>k.current_time());
    k.advance_to(t);
    REQUIRE(k.processed_certificate_failures()>=last_count);
    last_count=k.processed_certificate_failures();
    REQUIRE_EQ(k.minimum(),scan(a,t));
  }
  REQUIRE_THROWS_AS(k.advance_to(29),std::invalid_argument);
}
