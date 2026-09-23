# Scope recovery: Coffman–Graham two-processor scheduling

## Coverage decision

At the fresh base
`main@139ea9a5f6ea812b589b12af69fa3f1e9aff8698`, the repository had zero
open pull requests and zero open issues. The exact pull-request head that
produced that main commit had repository GCC release, Clang release, and GCC
ASan+UBSan CI green.

A fresh code / branch / pull-request audit found no Coffman–Graham,
two-processor precedence-scheduling, or equivalent implementation surface.

This slice therefore adds a new scheduling proof model rather than another
ordered-set, tree, string, or graph-recognition variant.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; the frozen compiler/backend history is
not resumed.

## Problem contract

Input is a directed graph represented as successor lists:

- task `u` must complete before task `v` when `u -> v`;
- every task takes exactly one unit of processing time;
- there are exactly two identical processors;
- tasks are non-preemptive;
- a task may start only in a slot strictly after every predecessor's slot.

The function returns:

- one unique Coffman–Graham label in `[1,n]` for every task;
- a deterministic sequence of unit-time slots;
- at most two tasks per slot.

For this classical special case, the resulting makespan is globally optimal.

Malformed input is rejected:

- successor index outside `[0,n)`;
- duplicate precedence edge;
- directed cycle, including a self-loop.

## Labeling convention

Different presentations sometimes reverse the numeric orientation, so this
implementation fixes one convention explicitly.

Labels are assigned `1,2,...,n` from sinks toward sources.

At a labeling step, a task is eligible exactly when all of its immediate
successors already have labels.

For each eligible task, production forms its signature:

1. collect the labels of all immediate successors;
2. sort those labels in decreasing numeric order;
3. compare signatures lexicographically.

The eligible task with the lexicographically smallest signature receives the
next label. Equal signatures break by smaller task index solely to make the
result deterministic.

The empty signature of a sink is lexicographically smaller than every nonempty
signature.

Because every predecessor becomes eligible only after all successors have
labels, every edge `u -> v` satisfies

`label(u) > label(v)`.

## Scheduling phase

After all labels are assigned, production performs the corresponding list
schedule in decreasing label order.

For each unit slot:

1. compute the currently ready unscheduled tasks;
2. choose the ready task with greatest label for processor 0;
3. if another ready task remains, choose the one with greatest label for
   processor 1;
4. only after the complete slot is fixed are successor indegrees decremented.

The fourth rule matters: a task made ready by a predecessor finishing in the
current slot may not execute in that same slot.

For unit-time precedence-constrained tasks on two processors, the
Coffman–Graham labeling plus this descending-label list schedule is the
classical optimal algorithm.

## Executable invariants

The returned schedule is required to satisfy all of the following:

- labels are a permutation of `1..n`;
- every task appears in exactly one slot;
- no slot contains more than two tasks;
- processor 1 is never occupied while processor 0 is empty;
- every precedence edge crosses strictly forward in time;
- every precedence edge crosses strictly downward in label:
  predecessor label greater than successor label;
- repeated calls on identical input return identical labels and slots.

The implementation does not depend on adjacency-list order for tie-breaking.

## Independent exact oracle

Committed optimality verification does not reuse Coffman–Graham labels.

For a small DAG, the oracle represents the set of completed tasks by a bitmask.

For every state it directly computes the currently ready tasks from predecessor
masks, then exhaustively tries:

- every one-task next slot;
- every two-task next slot.

The recurrence returns one plus the minimum remaining number of slots.
Memoization makes this an exact state-space search over completed-task subsets.

This oracle knows nothing about:

- lexicographic successor-label signatures;
- Coffman–Graham labels;
- descending-label priority.

It therefore gives independent evidence for makespan optimality.

Committed deterministic coverage includes all `2^10 = 1024` DAGs on five
vertices whose possible edges respect the natural topological order
`0 < 1 < ... < 4`.

Randomized coverage additionally generates 700 fixed-seed DAGs of up to eight
tasks after first shuffling vertex identities, then compares every produced
makespan with the exact bitmask optimum.

A supplementary model-level differential run checked 15,000 additional random
DAGs of sizes 0 through 9 against the same mathematical optimum using a separate
model implementation and found zero mismatch. That supporting model run is not
a compiler, sanitizer, or benchmark claim.

## Complexity boundary

Let:

- `n` be the number of tasks;
- `e` be the number of precedence edges;
- `Delta` be maximum outdegree.

Validation and reverse-edge construction use `O(n + e log Delta)` time under
the straightforward per-adjacency duplicate check.

Each task's successor-label signature is built once, for total
`O(e log Delta)` signature construction work.

This deliberately simple implementation scans the current eligible set for each
of the `n` label assignments. Comparing two signatures can cost
`O(Delta)`, so a conservative labeling bound is `O(n^2 Delta + e log Delta)`,
which is `O(n^3)` in the dense worst case.

The scheduling phase scans ready tasks to choose up to two greatest labels per
slot, giving a conservative `O(n^2 + e)` bound.

Production storage is `O(n + e)`, including reverse edges and the stored
successor-label signatures.

The exact bitmask scheduler is test-only and exponential.

This slice makes no claim that the implementation matches the best known
asymptotic implementations of the classical algorithm.

## Non-claims

This slice does **not** claim optimality for:

- three or more processors;
- non-unit processing times;
- processor-specific execution times;
- communication delays;
- release times or deadlines;
- preemptive jobs;
- cyclic precedence constraints.

It also does not claim benchmark-backed throughput or cache behavior.

## References

The algorithmic contract follows the classical two-processor unit-task
Coffman–Graham result:

- E. G. Coffman Jr. and R. L. Graham, *Optimal Scheduling for
  Two-Processor Systems*, Acta Informatica 1 (1972), 200–213.
- Ravi Sethi, *Scheduling Graphs on Two Processors*, SIAM Journal on Computing
  5(1) (1976), 73–82.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/graphs/coffman_graham.hpp`;
- `tests/test_coffman_graham_cases.hpp`;
- `docs/scope_recovery_coffman_graham.md`.

Pre-PR hardening includes:

- explicit inclusion of comparator utilities used by production;
- label iteration that does not depend on incrementing the maximum
  representable `size_t`;
- exhaustive five-node DAG optimality coverage.

No CMake, README, historical ROADMAP, workflow, benchmark, compiler/backend, or
temporary-file change is required.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base: `139ea9a5f6ea812b589b12af69fa3f1e9aff8698`.
