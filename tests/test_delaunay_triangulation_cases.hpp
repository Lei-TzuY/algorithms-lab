#pragma once

#include "test_framework.hpp"
#include "algorithms/geometry/delaunay_triangulation.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <set>
#include <span>
#include <utility>
#include <vector>

namespace {
using algorithms::geometry::DelaunayEdge;
using algorithms::geometry::DelaunayTriangle;
using algorithms::geometry::DelaunayTriangulation;
using algorithms::geometry::Point2i;
using algorithms::geometry::exact_delaunay_triangulation;

std::int64_t delaunay_test_orient2(Point2i a, Point2i b, Point2i c) {
  const std::int64_t x1 = static_cast<std::int64_t>(b.x) - a.x;
  const std::int64_t y1 = static_cast<std::int64_t>(b.y) - a.y;
  const std::int64_t x2 = static_cast<std::int64_t>(c.x) - a.x;
  const std::int64_t y2 = static_cast<std::int64_t>(c.y) - a.y;
  return x1 * y2 - y1 * x2;
}

struct DelaunayTestLifted { std::int64_t x; std::int64_t y; std::int64_t z; };
DelaunayTestLifted delaunay_test_lift(Point2i p) {
  const std::int64_t x = p.x;
  const std::int64_t y = p.y;
  return DelaunayTestLifted{x, y, x * x + y * y};
}

std::int64_t delaunay_test_orient3(Point2i a, Point2i b, Point2i c, Point2i d) {
  const DelaunayTestLifted A = delaunay_test_lift(a); const DelaunayTestLifted B = delaunay_test_lift(b);
  const DelaunayTestLifted C = delaunay_test_lift(c); const DelaunayTestLifted D = delaunay_test_lift(d);
  const std::int64_t ux=B.x-A.x, uy=B.y-A.y, uz=B.z-A.z;
  const std::int64_t vx=C.x-A.x, vy=C.y-A.y, vz=C.z-A.z;
  const std::int64_t wx=D.x-A.x, wy=D.y-A.y, wz=D.z-A.z;
  return ux*(vy*wz-vz*wy)-uy*(vx*wz-vz*wx)+uz*(vx*wy-vy*wx);
}

DelaunayTriangle delaunay_test_canonical(std::size_t i,std::size_t j,std::size_t k,
                           std::span<const Point2i> p) {
  if (delaunay_test_orient2(p[i],p[j],p[k]) > 0) return DelaunayTriangle{{i,j,k}};
  return DelaunayTriangle{{i,k,j}};
}

std::vector<DelaunayTriangle> delaunay_test_lifted_lower_hull_oracle(
    std::span<const Point2i> points) {
  std::vector<DelaunayTriangle> out;
  if (points.size() < 3) return out;
  for (std::size_t i=0;i<points.size();++i) for(std::size_t j=i+1;j<points.size();++j)
    for(std::size_t k=j+1;k<points.size();++k) {
      auto t=delaunay_test_canonical(i,j,k,points);
      bool lower=true;
      for(std::size_t p=0;p<points.size();++p){
        if(p==i||p==j||p==k) continue;
        if(delaunay_test_orient3(points[t.vertices[0]],points[t.vertices[1]],points[t.vertices[2]],points[p]) < 0){lower=false;break;}
      }
      if(lower) out.push_back(t);
    }
  return out;
}

std::vector<DelaunayEdge> delaunay_test_edges_from_triangles(
    const std::vector<DelaunayTriangle>& triangles) {
  std::vector<DelaunayEdge> edges;
  for(const auto&t:triangles){
    for(const auto& [a,b]:std::array<std::pair<std::size_t,std::size_t>,3>{
      std::pair{t.vertices[0],t.vertices[1]},std::pair{t.vertices[1],t.vertices[2]},
      std::pair{t.vertices[2],t.vertices[0]}}){auto [u,v]=std::minmax(a,b);edges.push_back({u,v});}
  }
  std::sort(edges.begin(),edges.end(),[](const auto&a,const auto&b){return std::pair{a.first,a.second}<std::pair{b.first,b.second};});
  edges.erase(std::unique(edges.begin(),edges.end()),edges.end());
  return edges;
}

bool delaunay_test_general_position(const std::vector<Point2i>& p){
 for(std::size_t i=0;i<p.size();++i) for(std::size_t j=i+1;j<p.size();++j) if(p[i]==p[j]) return false;
 for(std::size_t i=0;i<p.size();++i) for(std::size_t j=i+1;j<p.size();++j) for(std::size_t k=j+1;k<p.size();++k){
  if(delaunay_test_orient2(p[i],p[j],p[k])==0) return false;
  auto t=delaunay_test_canonical(i,j,k,p);
  for(std::size_t l=k+1;l<p.size();++l) if(delaunay_test_orient3(p[t.vertices[0]],p[t.vertices[1]],p[t.vertices[2]],p[l])==0) return false;
 }
 return true;
}

TEST_CASE(delaunay_small_and_boundary_contracts){
  REQUIRE(exact_delaunay_triangulation(std::span<const Point2i>{}).triangles.empty());
  const std::vector<Point2i> one{{7,-3}}; REQUIRE(exact_delaunay_triangulation(one).edges.empty());
  const std::vector<Point2i> two{{-2,5},{9,4}}; auto r2=exact_delaunay_triangulation(two);
  REQUIRE_EQ(r2.edges,std::vector<DelaunayEdge>({{0,1}}));
  const std::vector<Point2i> tri{{-10000,-10000},{10000,-9999},{0,10000}};
  auto rt=exact_delaunay_triangulation(tri); REQUIRE_EQ(rt.triangles.size(),std::size_t{1}); REQUIRE_EQ(rt.edges.size(),std::size_t{3});
  const std::vector<Point2i> bad{{10001,0}}; REQUIRE_THROWS_AS(exact_delaunay_triangulation(bad),std::out_of_range);
}

TEST_CASE(delaunay_general_position_rejections){
  const std::vector<Point2i> duplicate{{0,0},{1,2},{0,0}};
  REQUIRE_THROWS_AS(exact_delaunay_triangulation(duplicate),std::invalid_argument);
  const std::vector<Point2i> collinear{{0,0},{1,1},{2,2}};
  REQUIRE_THROWS_AS(exact_delaunay_triangulation(collinear),std::invalid_argument);
  const std::vector<Point2i> cocircular{{0,0},{2,0},{2,2},{0,2}};
  REQUIRE_THROWS_AS(exact_delaunay_triangulation(cocircular),std::invalid_argument);
}

TEST_CASE(delaunay_known_structure_and_determinism){
  const std::vector<Point2i> p{{0,0},{6,0},{1,5},{7,4},{3,2}};
  const auto expected=delaunay_test_lifted_lower_hull_oracle(p);
  const auto first=exact_delaunay_triangulation(p); const auto second=exact_delaunay_triangulation(p);
  REQUIRE_EQ(first,second); REQUIRE_EQ(first.triangles,expected); REQUIRE_EQ(first.edges,delaunay_test_edges_from_triangles(expected));
  for(const auto&t:first.triangles) REQUIRE(delaunay_test_orient2(p[t.vertices[0]],p[t.vertices[1]],p[t.vertices[2]])>0);
}

TEST_CASE(delaunay_randomized_lifted_lower_hull_differential){
  std::mt19937_64 rng(0xD31A'2026ULL);
  std::uniform_int_distribution<int> n_dist(3,8), coord(-40,40);
  for(int trial=0;trial<500;++trial){
    std::vector<Point2i> p;
    for(int attempt=0;attempt<10000;++attempt){
      p.clear(); const int n=n_dist(rng);
      for(int i=0;i<n;++i) p.push_back(Point2i{coord(rng),coord(rng)});
      if(delaunay_test_general_position(p)) break;
    }
    REQUIRE(delaunay_test_general_position(p));
    const auto actual=exact_delaunay_triangulation(p); const auto oracle=delaunay_test_lifted_lower_hull_oracle(p);
    REQUIRE_EQ(actual.triangles,oracle); REQUIRE_EQ(actual.edges,delaunay_test_edges_from_triangles(oracle));
    std::vector<bool> seen(p.size(),false);
    for(const auto&t:actual.triangles) for(auto v:t.vertices) seen[v]=true;
    for(bool present:seen) REQUIRE(present);
  }
}
}
