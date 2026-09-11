#include "algorithms/optimization/exact_linear_program.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using algorithms::optimization::LinearProgramResult;
using algorithms::optimization::LinearProgramStatus;
using algorithms::optimization::Rational64;

struct Rat {
  std::int64_t n = 0;
  std::int64_t d = 1;
  Rat() = default;
  Rat(std::int64_t num, std::int64_t den = 1) : n(num), d(den) {
    if (d == 0) throw std::runtime_error("zero denominator");
    if (d < 0) { n = -n; d = -d; }
    const auto g = std::gcd(n < 0 ? -n : n, d);
    if (g != 0) { n /= g; d /= g; }
  }
};

Rat as_rat(const Rational64& value) { return Rat(value.numerator, value.denominator); }
Rat add(Rat a, Rat b) { return Rat(a.n * b.d + b.n * a.d, a.d * b.d); }
Rat mul(Rat a, Rat b) { return Rat(a.n * b.n, a.d * b.d); }
int cmp(Rat a, Rat b) {
  const auto left = a.n * b.d;
  const auto right = b.n * a.d;
  return left < right ? -1 : (left > right ? 1 : 0);
}
bool le(Rat a, Rat b) { return cmp(a, b) <= 0; }
bool lt(Rat a, Rat b) { return cmp(a, b) < 0; }

Rat dot(const std::vector<std::int64_t>& coefficients,
        const std::vector<Rational64>& values) {
  Rat sum;
  for (std::size_t i = 0; i < coefficients.size(); ++i) {
    sum = add(sum, mul(Rat(coefficients[i]), as_rat(values[i])));
  }
  return sum;
}

void require_feasible(const std::vector<std::vector<std::int64_t>>& a,
                      const std::vector<std::int64_t>& b,
                      const std::vector<Rational64>& x) {
  for (const auto& value : x) REQUIRE(!lt(as_rat(value), Rat(0)));
  for (std::size_t row = 0; row < a.size(); ++row) {
    REQUIRE(le(dot(a[row], x), Rat(b[row])));
  }
}

void require_unbounded_ray(const std::vector<std::vector<std::int64_t>>& a,
                           const std::vector<std::int64_t>& c,
                           const LinearProgramResult& result) {
  REQUIRE_EQ(result.unbounded_direction.size(), c.size());
  for (const auto& value : result.unbounded_direction) {
    REQUIRE(!lt(as_rat(value), Rat(0)));
  }
  for (const auto& row : a) {
    REQUIRE(le(dot(row, result.unbounded_direction), Rat(0)));
  }
  REQUIRE(lt(Rat(0), dot(c, result.unbounded_direction)));
}

struct OneDimOracle {
  LinearProgramStatus status;
  Rat objective;
};

OneDimOracle solve_one_dim(const std::vector<std::vector<std::int64_t>>& a,
                           const std::vector<std::int64_t>& b,
                           std::int64_t objective) {
  Rat lower(0);
  std::optional<Rat> upper;
  for (std::size_t row = 0; row < a.size(); ++row) {
    const auto coeff = a[row][0];
    if (coeff == 0) {
      if (b[row] < 0) return {LinearProgramStatus::Infeasible, Rat(0)};
      continue;
    }
    Rat boundary(b[row], coeff);
    if (coeff > 0) {
      if (!upper || lt(boundary, *upper)) upper = boundary;
    } else {
      if (lt(lower, boundary)) lower = boundary;
    }
  }
  if (lt(lower, Rat(0))) lower = Rat(0);
  if (upper && lt(*upper, lower)) return {LinearProgramStatus::Infeasible, Rat(0)};
  if (objective > 0) {
    if (!upper) return {LinearProgramStatus::Unbounded, Rat(0)};
    return {LinearProgramStatus::Optimal, mul(Rat(objective), *upper)};
  }
  return {LinearProgramStatus::Optimal, mul(Rat(objective), lower)};
}

std::optional<Rat> bounded_2d_optimum(
    const std::vector<std::vector<std::int64_t>>& a,
    const std::vector<std::int64_t>& b,
    const std::vector<std::int64_t>& c) {
  struct Line { std::int64_t ax; std::int64_t ay; std::int64_t rhs; };
  std::vector<Line> lines;
  for (std::size_t i = 0; i < a.size(); ++i) lines.push_back({a[i][0], a[i][1], b[i]});
  lines.push_back({1, 0, 0});
  lines.push_back({0, 1, 0});

  std::optional<Rat> best;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    for (std::size_t j = i + 1; j < lines.size(); ++j) {
      const auto det = lines[i].ax * lines[j].ay - lines[j].ax * lines[i].ay;
      if (det == 0) continue;
      Rat x(lines[i].rhs * lines[j].ay - lines[j].rhs * lines[i].ay, det);
      Rat y(lines[i].ax * lines[j].rhs - lines[j].ax * lines[i].rhs, det);
      if (lt(x, Rat(0)) || lt(y, Rat(0))) continue;
      bool feasible = true;
      for (std::size_t row = 0; row < a.size(); ++row) {
        Rat lhs = add(mul(Rat(a[row][0]), x), mul(Rat(a[row][1]), y));
        if (!le(lhs, Rat(b[row]))) { feasible = false; break; }
      }
      if (!feasible) continue;
      Rat value = add(mul(Rat(c[0]), x), mul(Rat(c[1]), y));
      if (!best || lt(*best, value)) best = value;
    }
  }
  if (!best) best = Rat(0);
  return best;
}

TEST_CASE(exact_simplex_deterministic_statuses_and_witnesses) {
  {
    auto r = algorithms::optimization::maximize_linear_program(
        {{1,1},{1,0},{0,1}}, {4,2,3}, {3,2});
    REQUIRE_EQ(r.status, LinearProgramStatus::Optimal);
    REQUIRE_EQ(r.objective, (Rational64{10,1}));
    REQUIRE_EQ(r.variables, (std::vector<Rational64>{{2,1},{2,1}}));
    require_feasible({{1,1},{1,0},{0,1}}, {4,2,3}, r.variables);
  }
  {
    auto r = algorithms::optimization::maximize_linear_program(
        {{2,1},{1,2}}, {4,4}, {1,1});
    REQUIRE_EQ(r.status, LinearProgramStatus::Optimal);
    REQUIRE_EQ(r.objective, (Rational64{8,3}));
    REQUIRE_EQ(r.variables, (std::vector<Rational64>{{4,3},{4,3}}));
  }
  {
    auto r = algorithms::optimization::maximize_linear_program(
        {{-1},{1}}, {-2,5}, {1});
    REQUIRE_EQ(r.status, LinearProgramStatus::Optimal);
    REQUIRE(r.phase_one_used);
    REQUIRE_EQ(r.objective, (Rational64{5,1}));
    REQUIRE_EQ(r.variables, (std::vector<Rational64>{{5,1}}));
  }
  {
    auto r = algorithms::optimization::maximize_linear_program(
        {{1},{-1}}, {1,-2}, {1});
    REQUIRE_EQ(r.status, LinearProgramStatus::Infeasible);
    REQUIRE(r.phase_one_used);
  }
  {
    auto r = algorithms::optimization::maximize_linear_program({}, {}, {1,2});
    REQUIRE_EQ(r.status, LinearProgramStatus::Unbounded);
    require_feasible({}, {}, r.variables);
    require_unbounded_ray({}, {1,2}, r);
  }
  {
    auto r = algorithms::optimization::maximize_linear_program({{-1,0},{0,-1}}, {0,0}, {2,1});
    REQUIRE_EQ(r.status, LinearProgramStatus::Unbounded);
    require_feasible({{-1,0},{0,-1}}, {0,0}, r.variables);
    require_unbounded_ray({{-1,0},{0,-1}}, {2,1}, r);
  }
  {
    auto first = algorithms::optimization::maximize_linear_program(
        {{1,1},{1,0},{0,1}}, {1,1,1}, {1,1});
    auto second = algorithms::optimization::maximize_linear_program(
        {{1,1},{1,0},{0,1}}, {1,1,1}, {1,1});
    REQUIRE_EQ(first.status, LinearProgramStatus::Optimal);
    REQUIRE_EQ(first.objective, (Rational64{1,1}));
    REQUIRE_EQ(first.variables, second.variables);
    REQUIRE_EQ(first.pivot_count, second.pivot_count);
  }
  {
    auto r = algorithms::optimization::maximize_linear_program({{1}}, {7}, {0});
    REQUIRE_EQ(r.status, LinearProgramStatus::Optimal);
    REQUIRE_EQ(r.objective, (Rational64{0,1}));
    require_feasible({{1}}, {7}, r.variables);
  }
  {
    auto r = algorithms::optimization::maximize_linear_program({}, {}, {});
    REQUIRE_EQ(r.status, LinearProgramStatus::Optimal);
    REQUIRE_EQ(r.objective, (Rational64{0,1}));
    REQUIRE(r.variables.empty());
  }
}

TEST_CASE(exact_simplex_validation_and_overflow_fail_closed) {
  REQUIRE_THROWS_AS(algorithms::optimization::maximize_linear_program({{1}}, {}, {1}), std::invalid_argument);
  REQUIRE_THROWS_AS(algorithms::optimization::maximize_linear_program({{1,2}}, {3}, {1}), std::invalid_argument);
  REQUIRE_THROWS_AS(algorithms::optimization::maximize_linear_program({}, {}, {std::numeric_limits<std::int64_t>::min()}), std::overflow_error);
}

TEST_CASE(exact_simplex_random_one_dimensional_differential) {
  std::mt19937_64 rng(0x51A9E11ULL);
  std::uniform_int_distribution<int> row_dist(0, 7);
  std::uniform_int_distribution<int> coeff_dist(-3, 3);
  std::uniform_int_distribution<int> bound_dist(-6, 10);
  std::uniform_int_distribution<int> objective_dist(-5, 5);
  for (int trial = 0; trial < 1500; ++trial) {
    const int rows = row_dist(rng);
    std::vector<std::vector<std::int64_t>> a;
    std::vector<std::int64_t> b;
    for (int i = 0; i < rows; ++i) {
      a.push_back({coeff_dist(rng)});
      b.push_back(bound_dist(rng));
    }
    const std::int64_t c = objective_dist(rng);
    const auto oracle = solve_one_dim(a,b,c);
    const auto actual = algorithms::optimization::maximize_linear_program(a,b,{c});
    REQUIRE_EQ(actual.status, oracle.status);
    if (actual.status == LinearProgramStatus::Optimal) {
      REQUIRE_EQ(as_rat(actual.objective).n, oracle.objective.n);
      REQUIRE_EQ(as_rat(actual.objective).d, oracle.objective.d);
      require_feasible(a,b,actual.variables);
      REQUIRE_EQ(cmp(dot({c},actual.variables), as_rat(actual.objective)), 0);
    } else if (actual.status == LinearProgramStatus::Unbounded) {
      require_feasible(a,b,actual.variables);
      require_unbounded_ray(a,{c},actual);
    }
  }
}

TEST_CASE(exact_simplex_random_bounded_two_dimensional_vertex_oracle) {
  std::mt19937_64 rng(0x2D1A6A7ULL);
  std::uniform_int_distribution<int> extra_dist(0, 5);
  std::uniform_int_distribution<int> coeff_dist(-3, 3);
  std::uniform_int_distribution<int> bound_dist(0, 10);
  std::uniform_int_distribution<int> objective_dist(-5, 5);
  for (int trial = 0; trial < 900; ++trial) {
    std::vector<std::vector<std::int64_t>> a{{1,0},{0,1}};
    std::vector<std::int64_t> b{5,5};
    const int extras = extra_dist(rng);
    for (int i = 0; i < extras; ++i) {
      int x = coeff_dist(rng);
      int y = coeff_dist(rng);
      if (x == 0 && y == 0) x = 1;
      a.push_back({x,y});
      b.push_back(bound_dist(rng));
    }
    const std::vector<std::int64_t> c{objective_dist(rng), objective_dist(rng)};
    const Rat expected = *bounded_2d_optimum(a,b,c);
    const auto actual = algorithms::optimization::maximize_linear_program(a,b,c);
    REQUIRE_EQ(actual.status, LinearProgramStatus::Optimal);
    REQUIRE_EQ(cmp(as_rat(actual.objective), expected), 0);
    require_feasible(a,b,actual.variables);
    REQUIRE_EQ(cmp(dot(c,actual.variables), expected), 0);
  }
}

} // namespace
