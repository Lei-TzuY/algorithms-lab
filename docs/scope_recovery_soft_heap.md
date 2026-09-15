# Scope recovery — genuine soft heap with certified corruption budget

A fresh live-state audit after the pairing-heap checkpoint found no soft-heap
implementation in default-branch code, pull-request history, or branch names.
Recent slope-trick and pairing-heap checkpoints explicitly named soft heap as an
uncovered gap and rejected an exact heap with cosmetic corruption metadata as a
false capability claim. This slice closes that gap with an actual ranked-tree
soft heap whose node item lists share artificially raised current keys.

## Contract

`SoftHeap<T, Compare>` uses the repository heap comparator convention:
`Compare(a,b)` means `a` has higher priority than `b`.

The constructor takes an integer `error_rate_power = p >= 1`, representing the
exact dyadic error rate `epsilon = 2^-p`. This avoids floating-point rounding in
the proof contract. The rank threshold is `r = p + 5`; ranks at or below the
threshold have target list size one, while higher ranks use

`size(k) = ceil(3 * size(k-1) / 2)`.

Equal-rank trees are linked by a new parent. `sift` repeatedly steals the item
list of the better-current-key child until the parent reaches its target size or
becomes a leaf. When several original items share the raised parent current key,
items whose original key strictly outranks that current key are genuinely
corrupt. `pop` returns the original value, the current key used by the heap, and
an explicit corruption flag.

The heap exposes the lineage insertion count, live size, current corruption
count, the certified budget `floor(insertions / 2^p)`, and a structural
diagnostic. Destructive meld requires identical error-rate powers; stateful
comparator equivalence remains a caller precondition.

## Proof obligations and invariants

The implementation maintains:

1. Root trees have strictly increasing, distinct ranks after consolidation.
2. A parent current key is never worse-priority than a child's current key, so
   the root with best current key is a valid soft-heap minimum.
3. Every stored item's current key is never artificially improved relative to
   its original key.
4. Nodes at ranks `<= r` contain no corrupted items.
5. The Kaplan-Zwick sizing/threshold construction bounds the number of live
   corrupted items by `floor(I / 2^p)`, where `I` is the lineage's total number
   of insertions. The diagnostic recomputes this count from original/current
   keys rather than trusting a corruption counter.
6. Item ownership is conserved across list steals, extraction, move, and meld;
   destructive meld transfers the donor's insertion history with its live
   items and leaves the donor structurally empty.

The corruption guarantee is the defining capability. It is not inferred from a
boolean flag: current keys are actually raised by list sifting, and deterministic
tests exhibit both corrupted extractions and inversions in original-key output
order.

## Verification

Focused candidate bytes pass GCC C++20 strict warnings-as-errors, Clang C++20
strict warnings-as-errors, and actual GCC ASan+UBSan.

Executable evidence includes:

- invalid error-rate and empty-operation rejection;
- an exact low-rank regime before corruption can occur;
- a deterministic 512-key permutation at `epsilon=1/2` that produces real
  raised keys, corrupted items, and non-sorted original-key extraction while
  current keys remain monotone;
- destructive meld, self meld, move construction/assignment, mismatched-error
  rejection, and a custom max-order comparator;
- 6,000 mixed insert/pop/meld operations across six heaps against independent
  `std::multiset<int>` membership oracles;
- an additional 4,000-operation stress trace for each `p in {1,2,3,5}` that
  recomputes and checks the live corruption budget throughout.

The oracle deliberately does not demand exact-min extraction: that would erase
the approximation semantics being tested. It instead checks item conservation,
current-key monotonicity where applicable, structural invariants, and the actual
corruption theorem boundary.

## Complexity and non-claims

This implementation keeps the canonical ranked binary trees and list-sifting
corruption mechanism but uses a direct C++ root list with an explicit cached
minimum refresh. There are `O(log I)` roots after `I` lineage insertions, so this
baseline adds `O(log I)` root-list bookkeeping around operations in addition to
their sift/link work. It therefore deliberately does **not** claim the tightest
constant-amortized operation bounds of optimized soft-heap presentations.
Storage is linear in the insertion lineage, with pruning as empty leaves disappear.
Diagnostics are intentionally more expensive than normal operations.

No exact-priority-queue guarantee, arbitrary floating-point epsilon, decrease-key
handles, persistence, thread safety, comparator-equivalence detection,
benchmark-backed performance claim, or claim that corruption metadata alone
constitutes a soft heap is made.

`docs/scope_recovery_after_phase69.md` remains the prospective scope authority;
historical compiler/backend phases stay frozen.
