# Scope recovery: Ukkonen suffix tree byte index

## Coverage decision

After exact CYK byte-grammar parsing reached merged `main`, a fresh live-code,
pull-request-history, and branch audit found no suffix-tree / Ukkonen production
surface in `algorithms-lab`. The repository already contains sealed suffix-array,
suffix-automaton, BWT/FM-index-family, Aho-Corasick, and palindromic-tree work;
those structures answer related text queries but do not maintain Ukkonen's
compressed suffix-tree topology, active point, remaining-suffix state, and
suffix-link construction invariant. Historical Phase-5 and palindromic-tree
documents explicitly listed suffix trees as future/non-claimed work, and no
suffix-tree implementation PR or occupied suffix-tree branch was found.

This recovery follows `docs/scope_recovery_after_phase69.md`. It does not resume
the frozen compiler/backend sequence and does not edit the stale historical
ROADMAP presentation.

## Public contract

`SuffixTreeByteIndex(text)` builds an immutable suffix tree over arbitrary bytes.
Construction internally appends one sentinel symbol outside the byte alphabet,
so all suffixes are explicit while caller-visible semantics remain over bytes
`0..255` only.

The index exposes:

- `contains(pattern)` for exact substring membership;
- `occurrence_count(pattern)` from descendant real-leaf counts;
- `locate(pattern)` as sorted starting positions;
- `distinct_substring_count()` as the exact count of non-empty byte substrings;
- `node_count()` / `text_size()` diagnostics; and
- `valid_structure()`, an intentionally expensive replay diagnostic.

The empty pattern occurs at every boundary `0..text_size()`. NUL and high-bit
bytes use unsigned-byte semantics. The sentinel is never accepted as input and
never contributes to distinct-substring length or caller-visible positions.

## Ukkonen construction invariant

At phase end `p`, every suffix of the processed prefix that is not already
implicit in the tree is represented by `remaining`. The active point
`(active_node, active_edge, active_length)` names the locus of the next extension.
Each extension either creates a new leaf, splits one compressed edge and creates
an internal node plus leaf, or applies Rule 3 when the next symbol is already
present. Root adjustment and suffix-link traversal move the active point to the
next suffix without rescanning the full prefix.

Every non-root internal node receives a suffix link to the explicit node whose
path label is its own path label with the first symbol removed. The unique
sentinel makes every suffix terminate at a distinct leaf, which also gives each
leaf a stable suffix-start witness.

The implementation stores outgoing transitions in `std::map` for deterministic
iteration. With alphabet size at most 257, map lookup is bounded by a constant
alphabet factor; the code-local complexity is `O(n log sigma)` construction time
and `O(n)` nodes/storage, with `sigma <= 257`. This is linear in `n` for the fixed
byte-plus-sentinel alphabet, but the document keeps the comparison-map factor
explicit rather than claiming an implementation-independent constant.

## Query invariants

A substring query consumes whole or partial compressed edges from the root. If
the pattern ends in the middle of an edge, all descendant real leaves are still
exactly the occurrence starts for that pattern. Cached `real_leaf_count` is
therefore an exact occurrence count; `locate` replays descendant leaves and sorts
their suffix starts.

Every distinct non-empty byte substring corresponds to one prefix of one tree
edge before the sentinel. Summing sentinel-trimmed edge lengths over non-root
nodes therefore yields `distinct_substring_count()`.

Normal query costs are:

- `contains` / `occurrence_count`: `O(m log sigma)` for pattern length `m`;
- `locate`: `O(m log sigma + k + k log k)` for `k` returned positions (the final
  sort is deliberate for deterministic ascending output);
- resident tree storage: `O(n)` nodes plus deterministic transition maps.

`valid_structure()` is diagnostic, not a normal operation. It independently
checks topology, edge labels, leaf identities, cached leaf counts, suffix-link
semantics, and replays every suffix; its worst-case work may be quadratic.

## Independent verification

Primary query verification does not use suffix arrays, BWT, suffix automata, or
another suffix-tree implementation. It directly scans the original byte string.
Committed evidence covers:

- empty input, empty-pattern boundary semantics, `banana`, and repeated bytes;
- arbitrary NUL / `0xff` inputs;
- exact distinct-substring counts against explicit substring-set enumeration;
- 700 fixed-seed random texts of length 0..24 with 30 independent random queries
  per text;
- adversarial repeated / mostly-distinct shapes through length 96; and
- 120 arbitrary-byte texts of length 64..256 with 40 substring queries each.

For every constructed index `valid_structure()` also verifies each explicit
suffix leaf and each internal suffix-link target. Focused repo-native execution
passed strict GCC, strict Clang, and actual GCC ASan+UBSan builds before upload.

## Non-claims

This slice is not implemented through the existing suffix array, BWT/FM-index,
or suffix automaton. It does not claim a generalized suffix tree, dynamic
post-construction appends/deletions, succinct/compressed representation, suffix-
tree-based LCP/RMQ, longest-common-substring API, or fastest practical suffix-tree
performance. `std::map` transitions are chosen for proof transparency and
deterministic traversal, not cache-optimal throughput.

## Scope

The intended review surface is exactly five files:

- one CMake source/test registration pair;
- `include/algorithms/strings/suffix_tree.hpp`;
- `src/strings/suffix_tree.cpp`;
- `tests/test_suffix_tree.cpp`; and
- this focused recovery proof document.

No README, ROADMAP, scope-recovery authority, workflow, benchmark, frozen
compiler/backend, or temporary-file churn belongs to this slice.
