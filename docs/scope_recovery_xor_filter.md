# Scope recovery — static XOR fingerprint filter

## Coverage decision

A fresh live-state audit at `main@cc2614de6f5fddc7363d7c77c08130e1a2a6c2b1`
found no XOR-filter / peelable-fingerprint-filter implementation in default-branch
code, open pull requests, or the recovery branch namespace. The repository
already contains an exact Robin-Hood set, cuckoo hashing, FKS perfect hashing,
and a quotient filter, so this slice is intentionally not another mutable hash
set. It adds a different static approximate-membership construction whose core
obligation is successful peeling of a three-way hypergraph and reverse
fingerprint assignment.

The stale historical compiler/backend ROADMAP tail remains non-authoritative;
prospective scope continues to follow `scope_recovery_after_phase69.md` and fresh
coverage audits.

## Production contract

`Xor8Filter64` builds an immutable set over 64-bit keys.

- Input duplicates are collapsed to set semantics before construction.
- Empty input produces an empty filter and every query returns false.
- A non-empty key maps to one slot in each of three disjoint blocks.
- Each slot stores one 8-bit fingerprint byte.
- Construction repeatedly tries deterministic seeds derived from the caller's
  base seed. Every attempt builds the three-uniform incidence hypergraph,
  peels degree-one vertices, and succeeds only when every key-edge is peeled.
- If the caller's bounded attempt budget is exhausted, construction throws; an
  unpeelable core is never silently accepted.
- Reverse peel order assigns exactly one slot per peeled edge so that the XOR of
  its three resident bytes equals that key's fingerprint.
- `contains()` performs exactly those three reads and one 8-bit fingerprint
  comparison.
- The selected seed, requested seed, attempt count, slot count, and resident
  fingerprint bytes are replayable diagnostics.

The baseline deliberately provisions about two resident byte slots per distinct
key (three equal blocks, with small-size rounding) rather than claiming the
space constant of more aggressively tuned XOR-filter implementations. That
extra slack made construction reliability an executable property instead of a
hidden retry assumption.

## Correctness obligation

For a successful peel sequence, process the recorded edges in reverse. When an
edge is reconstructed, its designated peeled vertex has not yet been assigned,
while the other two resident slots already contain their final values. Assigning

`slot[v] = fingerprint(key) XOR slot[a] XOR slot[b]`

therefore establishes the query equation for that edge. A later reverse step can
only write the unique vertex designated for a different peeled edge, so the
already-established equation is preserved. Induction over the reverse peel order
proves that every inserted distinct key is reported present.

This is a **no-false-negative** statement for successfully constructed filters,
not exact membership. A non-member may satisfy the same 8-bit XOR equation and
be reported present.

## Verification

Focused verification uses the repository warning policy with GCC and Clang plus
an actual GCC ASan+UBSan build.

Deterministic cases cover:

- empty input and invalid zero attempt budgets;
- duplicate collapse and insertion-order-independent replay;
- a fixed 80-key corpus whose first seed leaves an unpeelable core, proving that
  a one-attempt build throws while the deterministic second attempt succeeds;
- an explicit non-member false positive, making the approximate-membership
  boundary executable rather than documentation-only.

Randomized verification builds 300 fixed-seed distinct-key sets of varying size,
checks every inserted key for false negatives, validates resident slot accounting,
and rebuilds each set to require identical selected seeds, attempt counts, and
fingerprint arrays.

The first focused prototype used roughly 1.5 resident slots per key and failed a
fixed randomized corpus even after 256 deterministic retries. That production
configuration was rejected rather than weakening the test; the sealed candidate
uses the more conservative roughly-2n resident-slot baseline.

## Complexity and non-claims

Let `n` be the number of distinct input keys and `A` the number of construction
attempts actually used. Deterministic duplicate normalization costs
`O(n log n)`. Each peel attempt is `O(n)` time and `O(n)` construction workspace,
so total build time is `O(n log n + A n)`. The resident filter uses `O(n)` bytes
of fingerprint state, and each membership query is `O(1)` time.

The deterministic mixer is not cryptographic. No adversarial false-positive
bound, cryptographic property, random-oracle proof, construction-success
probability, or exact-membership claim is made. The implementation is a
first-principles correctness baseline for peelable static fingerprint filters,
not a claim of state-of-the-art XOR-filter space constants or build throughput.
