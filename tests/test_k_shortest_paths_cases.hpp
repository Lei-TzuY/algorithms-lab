#pragma once

#include "test_framework.hpp"
#include "algorithms/graphs/k_shortest_paths.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

using algorithms::graphs::DirectedEdgeRef;
using algorithms::graphs::Graph;
using algorithms::graphs::LooplessPath;
using algorithms::graphs::Vertex;
using algorithms::graphs::Weight;
using algorithms::graphs::yen_k_shortest_loopless_paths;

namespace {
struct OraclePath { Weight cost{}; std::vector<DirectedEdgeRef> edges; std::vector<Vertex> vertices; };

void enumerate_paths(const Graph& graph, Vertex current, Vertex target,
                     std::vector<bool>& used, Weight cost,
                     std::vector<DirectedEdgeRef>& edges,
                     std::vector<Vertex>& vertices,
                     std::vector<OraclePath>& output) {
  if (current == target) { output.push_back({cost, edges, vertices}); return; }
  const auto& nbrs = graph.neighbors(current);
  for (std::size_t i=0;i<nbrs.size();++i) {
    const auto& edge=nbrs[i];
    if (used[edge.to]) continue;
    if (edge.weight > 0 && cost > std::numeric_limits<Weight>::max()-edge.weight) continue;
    used[edge.to]=true;
    edges.push_back({current,i});
    vertices.push_back(edge.to);
    enumerate_paths(graph,edge.to,target,used,cost+edge.weight,edges,vertices,output);
    vertices.pop_back(); edges.pop_back(); used[edge.to]=false;
  }
}

std::vector<OraclePath> oracle_paths(const Graph& graph, Vertex source, Vertex target) {
  std::vector<OraclePath> out;
  std::vector<bool> used(graph.vertex_count(),false);
  used[source]=true;
  std::vector<DirectedEdgeRef> edges;
  std::vector<Vertex> vertices{source};
  enumerate_paths(graph,source,target,used,0,edges,vertices,out);
  std::sort(out.begin(),out.end(),[](const OraclePath&a,const OraclePath&b){
    if(a.cost!=b.cost)return a.cost<b.cost;
    const std::size_t m=std::min(a.edges.size(),b.edges.size());
    for(std::size_t i=0;i<m;++i){
      if(a.edges[i].from!=b.edges[i].from)return a.edges[i].from<b.edges[i].from;
      if(a.edges[i].adjacency_index!=b.edges[i].adjacency_index)return a.edges[i].adjacency_index<b.edges[i].adjacency_index;
    }
    return a.edges.size()<b.edges.size();
  });
  return out;
}

void validate_path(const Graph& graph,const LooplessPath& path,Vertex source,Vertex target){
  REQUIRE(!path.vertices.empty());
  REQUIRE_EQ(path.vertices.front(),source); REQUIRE_EQ(path.vertices.back(),target);
  REQUIRE_EQ(path.edges.size()+1U,path.vertices.size());
  std::set<Vertex> seen;
  Weight total=0;
  for(std::size_t i=0;i<path.vertices.size();++i) REQUIRE(seen.insert(path.vertices[i]).second);
  for(std::size_t i=0;i<path.edges.size();++i){
    const auto& r=path.edges[i]; REQUIRE_EQ(r.from,path.vertices[i]);
    REQUIRE(r.adjacency_index<graph.neighbors(r.from).size());
    const auto&e=graph.neighbors(r.from)[r.adjacency_index]; REQUIRE_EQ(e.to,path.vertices[i+1]);
    REQUIRE(e.weight>=0); REQUIRE(total<=std::numeric_limits<Weight>::max()-e.weight); total+=e.weight;
  }
  REQUIRE_EQ(total,path.total_weight);
}
}

TEST_CASE(yen_validation_and_trivial_cases){
  Graph undirected(2,false);undirected.add_edge(0,1,1); REQUIRE_THROWS_AS(yen_k_shortest_loopless_paths(undirected,0,1,1),std::invalid_argument);
  Graph graph(3,true); graph.add_edge(0,1,1); graph.add_edge(2,2,-1);
  REQUIRE_THROWS_AS(yen_k_shortest_loopless_paths(graph,0,1,1),std::invalid_argument);
  Graph clean(3,true); clean.add_edge(0,1,1);
  REQUIRE_THROWS_AS(yen_k_shortest_loopless_paths(clean,3,1,1),std::out_of_range);
  REQUIRE(yen_k_shortest_loopless_paths(clean,0,2,5).empty());
  REQUIRE(yen_k_shortest_loopless_paths(clean,0,1,0).empty());
  const auto self=yen_k_shortest_loopless_paths(clean,1,1,5); REQUIRE_EQ(self.size(),1U); REQUIRE_EQ(self[0].total_weight,0); REQUIRE_EQ(self[0].vertices,std::vector<Vertex>({1}));
}

TEST_CASE(yen_parallel_edges_are_distinct_paths){
  Graph g(3,true); g.add_edge(0,1,1); g.add_edge(0,1,2); g.add_edge(1,2,1); g.add_edge(0,2,4);
  const auto paths=yen_k_shortest_loopless_paths(g,0,2,10); REQUIRE_EQ(paths.size(),3U);
  REQUIRE_EQ(paths[0].total_weight,2); REQUIRE_EQ(paths[1].total_weight,3); REQUIRE_EQ(paths[2].total_weight,4);
  for(const auto&p:paths) validate_path(g,p,0,2);
  REQUIRE(!(paths[0].edges==paths[1].edges));
}

TEST_CASE(yen_tied_paths_are_deterministic_and_complete){
  Graph g(4,true); g.add_edge(0,1,1);g.add_edge(0,2,2);g.add_edge(1,3,2);g.add_edge(2,3,1);g.add_edge(1,2,1);g.add_edge(0,3,5);
  const auto a=yen_k_shortest_loopless_paths(g,0,3,10); const auto b=yen_k_shortest_loopless_paths(g,0,3,10); REQUIRE_EQ(a,b); REQUIRE_EQ(a.size(),4U);
  REQUIRE_EQ(a[0].total_weight,3);REQUIRE_EQ(a[1].total_weight,3);REQUIRE_EQ(a[2].total_weight,3);REQUIRE_EQ(a[3].total_weight,5);
  for(const auto&p:a)validate_path(g,p,0,3);
}

TEST_CASE(yen_path_cost_overflow_fails_closed){
  Graph g(3,true);g.add_edge(0,1,std::numeric_limits<Weight>::max());g.add_edge(1,2,1);
  REQUIRE_THROWS_AS(yen_k_shortest_loopless_paths(g,0,2,1),std::overflow_error);
}

TEST_CASE(yen_randomized_differential_against_exhaustive_simple_paths){
  std::mt19937_64 rng(0x59454E5F4B5350ULL);
  for(std::size_t trial=0;trial<500;++trial){
    const std::size_t n=2U+static_cast<std::size_t>(rng()%6U); Graph g(n,true);
    for(Vertex u=0;u<n;++u){
      for(Vertex v=0;v<n;++v){
        if((rng()%100U)<24U){g.add_edge(u,v,static_cast<Weight>(rng()%10U)); if((rng()%100U)<12U)g.add_edge(u,v,static_cast<Weight>(rng()%10U));}
      }
    }
    const Vertex s=static_cast<Vertex>(rng()%n); Vertex t=static_cast<Vertex>(rng()%n); if(t==s)t=(t+1U)%n;
    const std::size_t k=1U+static_cast<std::size_t>(rng()%12U);
    const auto oracle=oracle_paths(g,s,t); const auto actual=yen_k_shortest_loopless_paths(g,s,t,k);
    const std::size_t expected=std::min(k,oracle.size()); REQUIRE_EQ(actual.size(),expected);
    for(std::size_t i=0;i<actual.size();++i){ validate_path(g,actual[i],s,t); REQUIRE_EQ(actual[i].total_weight,oracle[i].cost); if(i>0)REQUIRE(actual[i-1].total_weight<=actual[i].total_weight); }
    for(std::size_t i=0;i<actual.size();++i)for(std::size_t j=i+1;j<actual.size();++j)REQUIRE(!(actual[i].edges==actual[j].edges));
  }
}
