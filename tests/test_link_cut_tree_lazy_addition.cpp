#include "algorithms/graphs/link_cut_tree.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

using algorithms::graphs::LinkCutForest;
using algorithms::graphs::Vertex;

namespace {
class NaiveValuedForest {
 public:
  explicit NaiveValuedForest(std::size_t n)
      : adjacency_(n, std::vector<bool>(n, false)), values_(n, 0) {}
  [[nodiscard]] std::size_t vertex_count() const noexcept { return values_.size(); }
  [[nodiscard]] bool has_edge(Vertex a, Vertex b) const { return adjacency_[a][b]; }
  void link(Vertex a, Vertex b){adjacency_[a][b]=true;adjacency_[b][a]=true;}
  void cut(Vertex a, Vertex b){adjacency_[a][b]=false;adjacency_[b][a]=false;}
  void assign_value(Vertex v,std::int64_t x){values_[v]=x;}
  [[nodiscard]] std::optional<std::vector<Vertex>> path(Vertex a,Vertex b) const {
    constexpr Vertex none=std::numeric_limits<Vertex>::max();
    std::vector<Vertex> parent(vertex_count(),none);std::queue<Vertex> q;parent[a]=a;q.push(a);
    while(!q.empty()){Vertex v=q.front();q.pop();if(v==b)break;for(Vertex u=0;u<vertex_count();++u){if(adjacency_[v][u]&&parent[u]==none){parent[u]=v;q.push(u);}}}
    if (parent[b] == none) {
      return std::nullopt;
    }
    std::vector<Vertex> out;
    for (Vertex v = b;; v = parent[v]) {
      out.push_back(v);
      if (v == a) {
        break;
      }
    }
    std::reverse(out.begin(), out.end());
    return out;
  }
  [[nodiscard]] bool connected(Vertex a,Vertex b) const{return path(a,b).has_value();}
  [[nodiscard]] std::size_t distance(Vertex a,Vertex b) const {auto p=path(a,b);if(!p)throw std::invalid_argument("disconnected");return p->size()-1;}
  void assign_path(Vertex a,Vertex b,std::int64_t x){auto p=path(a,b);if(!p)throw std::invalid_argument("disconnected");for(Vertex v:*p)values_[v]=x;}
  void add_path(Vertex a,Vertex b,std::int64_t delta){auto p=path(a,b);if(!p)throw std::invalid_argument("disconnected");for(Vertex v:*p){if(delta>0&&values_[v]>std::numeric_limits<std::int64_t>::max()-delta)throw std::overflow_error("overflow");if(delta<0&&values_[v]<std::numeric_limits<std::int64_t>::min()-delta)throw std::overflow_error("overflow");}for(Vertex v:*p)values_[v]+=delta;}
  [[nodiscard]] std::int64_t path_sum(Vertex a,Vertex b) const {auto p=path(a,b);if(!p)throw std::invalid_argument("disconnected");std::int64_t sum=0;for(Vertex v:*p)sum+=values_[v];return sum;}
  [[nodiscard]] std::int64_t value(Vertex v) const{return values_[v];}
  [[nodiscard]] std::vector<std::pair<Vertex,Vertex>> edges() const {std::vector<std::pair<Vertex,Vertex>> out;for(Vertex a=0;a<vertex_count();++a)for(Vertex b=a+1;b<vertex_count();++b)if(adjacency_[a][b])out.emplace_back(a,b);return out;}
 private: std::vector<std::vector<bool>> adjacency_;std::vector<std::int64_t> values_;
};
Vertex random_vertex(std::mt19937_64& g,std::size_t n){return static_cast<Vertex>(g()%static_cast<std::uint64_t>(n));}
std::int64_t random_value(std::mt19937_64&g){return static_cast<std::int64_t>(g()%2000001ULL)-1000000;}
std::int64_t random_delta(std::mt19937_64&g){return static_cast<std::int64_t>(g()%20001ULL)-10000;}
}

TEST_CASE(link_cut_path_addition_assignment_order_and_topology) {
  LinkCutForest f(6);for(Vertex v=0;v<6;++v)f.assign_value(v,static_cast<std::int64_t>(v+1));for(Vertex v=0;v+1<6;++v)f.link(v,v+1);
  f.assign_path_value(1,4,10);f.add_path_value(2,5,3);REQUIRE_EQ(f.path_sum(0,5),static_cast<std::int64_t>(59));
  f.add_path_value(5,1,-4);REQUIRE_EQ(f.path_sum(0,5),static_cast<std::int64_t>(39));
  f.assign_path_value(3,5,-7);REQUIRE_EQ(f.path_sum(0,5),static_cast<std::int64_t>(-5));
  f.add_path_value(0,4,2);REQUIRE_EQ(f.path_sum(0,5),static_cast<std::int64_t>(5));
  f.cut(2,3);REQUIRE_EQ(f.path_sum(0,2),static_cast<std::int64_t>(22));REQUIRE_EQ(f.path_sum(3,5),static_cast<std::int64_t>(-17));
  f.link(0,5);REQUIRE_EQ(f.path_sum(2,4),static_cast<std::int64_t>(10));REQUIRE(f.valid_auxiliary_invariants());
}

TEST_CASE(link_cut_path_addition_transactional_int64_boundaries) {
  const auto hi=std::numeric_limits<std::int64_t>::max();const auto lo=std::numeric_limits<std::int64_t>::min();
  LinkCutForest f(4);f.link(0,1);f.link(1,2);f.link(2,3);f.assign_value(0,hi-1);f.assign_value(1,7);f.assign_value(2,-9);f.assign_value(3,lo+2);
  f.add_path_value(0,3,1);REQUIRE_EQ(f.path_sum(0,0),hi);REQUIRE_EQ(f.path_sum(3,3),lo+3);
  REQUIRE_THROWS_AS(f.add_path_value(0,3,1),std::overflow_error);REQUIRE_EQ(f.path_sum(0,0),hi);REQUIRE_EQ(f.path_sum(1,1),static_cast<std::int64_t>(8));REQUIRE_EQ(f.path_sum(3,3),lo+3);
  REQUIRE_THROWS_AS(f.add_path_value(2,3,-4),std::overflow_error);REQUIRE_EQ(f.path_sum(2,2),static_cast<std::int64_t>(-8));REQUIRE_EQ(f.path_sum(3,3),lo+3);REQUIRE(f.valid_auxiliary_invariants());
}

TEST_CASE(link_cut_path_addition_wide_lazy_composition_and_cancellation) {
  const auto hi=std::numeric_limits<std::int64_t>::max();const auto lo=std::numeric_limits<std::int64_t>::min();
  LinkCutForest f(5);for(Vertex v=0;v<5;++v){f.assign_value(v,hi);if(v!=0)f.link(v-1,v);}
  f.add_path_value(0,4,lo);for(Vertex v=0;v<5;++v)REQUIRE_EQ(f.path_sum(v,v),static_cast<std::int64_t>(-1));
  f.assign_path_value(0,4,hi);f.add_path_value(0,4,lo);f.add_path_value(0,4,lo+1);for(Vertex v=0;v<5;++v)REQUIRE_EQ(f.path_sum(v,v),lo);
  f.assign_path_value(0,4,lo);f.add_path_value(0,4,hi);f.add_path_value(0,4,hi);for(Vertex v=0;v<5;++v)REQUIRE_EQ(f.path_sum(v,v),hi-1);
  f.assign_path_value(0,4,0);f.assign_value(0,hi);f.assign_value(1,lo);f.assign_value(2,1);REQUIRE_EQ(f.path_sum(0,2),static_cast<std::int64_t>(0));REQUIRE(f.valid_auxiliary_invariants());
}

TEST_CASE(link_cut_path_addition_validation_and_randomized_differential) {
  LinkCutForest bad(3);bad.link(0,1);REQUIRE_THROWS_AS(bad.add_path_value(0,2,1),std::invalid_argument);REQUIRE_THROWS_AS(bad.add_path_value(0,3,1),std::out_of_range);
  constexpr std::size_t n=40, operations=30000;LinkCutForest f(n);NaiveValuedForest oracle(n);std::mt19937_64 g(0x37AFF1AEULL);
  for(std::size_t op=0;op<operations;++op){const auto choice=g()%100ULL;Vertex a=random_vertex(g,n),b=random_vertex(g,n);
    if(choice<13){bool ok=a!=b&&!oracle.connected(a,b);if(ok){f.link(a,b);oracle.link(a,b);}else REQUIRE_THROWS_AS(f.link(a,b),std::invalid_argument);}
    else if(choice<23){auto edges=oracle.edges();bool use=!edges.empty()&&(g()%4ULL!=0ULL);Vertex x=a,y=b;if(use){auto e=edges[static_cast<std::size_t>(g()%edges.size())];x=e.first;y=e.second;if(g()&1ULL)std::swap(x,y);}if(x!=y&&oracle.has_edge(x,y)){f.cut(x,y);oracle.cut(x,y);}else REQUIRE_THROWS_AS(f.cut(x,y),std::invalid_argument);}
    else if(choice<36){auto x=random_value(g);f.assign_value(a,x);oracle.assign_value(a,x);}
    else if(choice<50){auto x=random_value(g);if(oracle.connected(a,b)){f.assign_path_value(a,b,x);oracle.assign_path(a,b,x);}else REQUIRE_THROWS_AS(f.assign_path_value(a,b,x),std::invalid_argument);}
    else if(choice<68){auto d=random_delta(g);if(oracle.connected(a,b)){f.add_path_value(a,b,d);oracle.add_path(a,b,d);}else REQUIRE_THROWS_AS(f.add_path_value(a,b,d),std::invalid_argument);}
    else if(choice<77){REQUIRE_EQ(f.connected(a,b),oracle.connected(a,b));}
    else if(choice<86){if(oracle.connected(a,b))REQUIRE_EQ(f.path_edge_distance(a,b),oracle.distance(a,b));else REQUIRE_THROWS_AS(f.path_edge_distance(a,b),std::invalid_argument);}
    else {if(oracle.connected(a,b))REQUIRE_EQ(f.path_sum(a,b),oracle.path_sum(a,b));else REQUIRE_THROWS_AS(f.path_sum(a,b),std::invalid_argument);}
    REQUIRE(f.valid_auxiliary_invariants());
  }
}
