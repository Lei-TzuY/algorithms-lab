# Scope recovery: online dynamic minimum spanning forest

## Fresh coverage decision

This recovery slice targets a live post-Phase-69 gap rather than extending the frozen historical compiler/backend roadmap. The prospective authority remains `docs/scope_recovery_after_phase69.md` plus fresh live code / PR / branch audits.

The repository already has sealed static Kruskal/Prim minimum spanning forests, first-principles Link-Cut and Euler-tour dynamic forests, and online fully dynamic general connectivity. Fresh PR-history searches found no dynamic-MST / dynamic-MSF implementation. The Euler-tour-forest recovery explicitly left dynamic minimum spanning forest out of scope, and the fully-dynamic-connectivity recovery likewise states that it does not provide dynamic MST. The long-lived `scope-recovery-minimum-cycle-basis` surface remains untouched.

## Production contract

`DynamicMinimumSpanningForest` maintains an exact minimum spanning forest of a fixed dense undirected vertex universe under online weighted multigraph edge insertions and removals.

- every insertion returns a stable edge handle; handles are never reused;
- signed `int64_t` edge weights are supported;
- parallel copies remain distinct by handle;
- self-loops remain active edge copies but are never forest edges;
- removal is by exact handle and rejects unknown / already-inactive handles;
- the current forest exposes exact active/tree membership, edge witness, connectivity, component count, and deterministic forest-edge handles;
- equal-weight insertion does not replace an already selected tree edge, while deletion replacement chooses minimum `(weight, handle)` for deterministic replay;
- public total forest weight is exact when representable as `int64_t` and throws only when the final mathematical total lies outside that range.

## Update invariant and proof boundary

The maintained selected edges form a minimum spanning forest of the complete active multigraph.

### Insertion

If a non-loop inserted edge joins two different current forest components, it is the only newly available relation between those previously disconnected active components and is selected.

Otherwise its endpoints already have a unique forest path. Adding the edge creates one cycle. If the new edge is strictly lighter than the heaviest edge on that path, production exchanges those two edges. By the classical MST cycle property, removing a heaviest edge of the cycle preserves existence of a minimum spanning forest and strictly improves the selected weight when the new edge is lighter. If it is not lighter, the existing forest remains minimum.

### Deletion

Deleting a self-loop or non-tree edge cannot invalidate the selected forest. Deleting a tree edge splits one represented tree into two sides. Production scans every remaining active non-tree copy and promotes the minimum `(weight, handle)` edge crossing that cut, if one exists. By the cut property, a lightest crossing edge is safe for a minimum spanning forest. If no crossing edge exists, the active graph itself is disconnected across the cut.

### Executable optimality diagnostic

`valid_structure()` independently replays forest acyclicity and active-graph component equivalence. For every active non-tree non-loop edge `(u,v,w)`, it also reconstructs the unique forest path and requires the maximum selected edge weight on that path to be at most `w`. This is the standard cycle-optimality characterization of a minimum spanning forest.

The cycle/cut theorems are proof obligations. Randomized tests are implementation evidence, not substitutes for those theorems.

## Exact total-weight arithmetic

Naive incremental `int64_t` accumulation would falsely reject cancellation cases such as `INT64_MAX + 1 - 1`. Production therefore accumulates selected weights in a private fixed two-limb signed-magnitude integer and narrows only at the public `total_weight()` boundary.

On supported targets `size_t` has at most 64 value bits. A forest contains fewer than `SIZE_MAX` edges, and each signed-64 edge has magnitude at most `2^63`; every structurally realizable forest total therefore has magnitude below `2^127`, which fits the private two-limb representation. Exact `INT64_MIN` and `INT64_MAX` are accepted; only a genuinely out-of-range final total throws.

## Verification

Focused repo-native verification on the candidate bytes passed:

- GCC C++20 strict warnings-as-errors;
- Clang C++20 strict warnings-as-errors;
- actual GCC ASan+UBSan with halt-on-error settings.

Deterministic evidence covers cycle replacement, deletion replacement, parallel copies, self-loops, signed weights, stale/invalid handles and vertices, exact `INT64_MIN`, positive overflow, and cancellation back to exact `INT64_MAX`.

The primary randomized oracle is structurally independent of the online cycle/cut maintenance algorithm. For bounded active multigraphs it enumerates every candidate edge subset of the required cardinality, rejects cyclic/non-spanning subsets, and selects the exact minimum total. Across 240 fixed-seed traces of 90 mixed updates on graphs with up to six vertices and at most eleven stored edge handles, production component count and total weight must match this exhaustive optimum after every update; the structural diagnostic must also hold.

A separate local stress check compared the private exact total against arbitrary-precision arithmetic on extreme `INT64_MIN` / `INT64_MAX` mixtures. That stress check is implementation evidence only and is not required by repository CI.

## Complexity and non-claims

This is deliberately a direct educational online baseline, not a Holm-de Lichtenberg-Thorup-style dynamic MST structure.

Forest adjacency is rebuilt for path/component operations and deletion replacement scans active non-tree edges. A conservative bound for one update is `O(V + E)` time and `O(V + E)` temporary/resident storage beyond the edge table. `valid_structure()` is an expensive diagnostic and may take `O(E(V+E))` time because it replays path optimality for every non-tree edge.

No polylogarithmic update bound, deterministic worst-case logarithmic bound, Link-Cut/Euler-tour acceleration, dynamic-tree benchmark speedup, or canonical choice among all equal-weight MSFs is claimed.
