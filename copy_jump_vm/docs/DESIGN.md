# Copy-Jump VM — design

## Machine

- **Memory:** `N` cells, each holding a value `0..V-1` (default N=16, V=16 — the
  "nibble" size class).
- **State:** (program counter, memory contents) — **finitely many**
  configurations (`V^N`).
- **Instructions:** `poke addr val`, `copy dst src`, `peek addr`, `jz addr target`,
  `jump target`, `halt`.

## Decidability

Because the configuration space is finite, the machine is a finite automaton:

- simulate and detect a repeated configuration → loop (non-termination);
- reach `halt` → terminates.

So halting/liveness is **decidable** — the bounded-tape result (§19). A real
machine with M bits has ≤ 2^M states: decidable in principle, infeasible to
enumerate — but for *small* N the reachable states are few and directly
enumerable.

## Reclamation (liveness)

`poke` allocates, `peek`/`copy`-src uses. A cell is **live** from its last `poke`
until its last `peek`; afterward it is dead and reusable. The liveness pass
computes this by exhaustive reachability (§21–22): enumerate all reachable states
and compute, per state, the set of cells read on any future path (backward
liveness fixed point).

## Corollaries

- **Address range dominates:** state space `V^N = 2^(V·2^A)`; `A` (address bits)
  is double-exponential, `V` (value bits) is linear.
- **Exact vs. conservative:** for tiny `N` the reachable states are few and
  enumerable → exact reclamation; for large `N` the reachable states explode →
  fall back to conservative rules (regions/ownership, §17).
- **Reclamation is a scaling problem:** at `N=256` you could pre-allocate
  everything and never reclaim (§21).

## Relationship to the stable cell

The stable cell (`../stable_cell/`) is the *rewiring* primitive (axis 2). This VM
is the *lifetime* substrate (axis 1): `peek`/`poke` make every access statically
visible, so the compiler can place frees by construction (§20).
