# Stable Cell — the `[t]` / `c[0]` / `c[0] = t` pattern

The **stable cell** is the primitive underneath the repo's "mutable chain"
technique: a one-slot mutable container used as a box for a single reference,
whose *identity* never changes while its *contents* (what it points to) can be
rewired at any time.

## The spec

| operation | Python / JS / Ruby | Nim |
|-----------|--------------------|-----|
| make a cell holding `t` | `[t]` | `make(t)` → `Cell[T](value: t)` |
| read the current target | `c[0]` | `get(c)` |
| re-point the cell to `t` | `c[0] = t` | `setCell(c, t)` |

## Invariants

- **I1 Identity stability** — `set` never changes the cell's object identity.
- **I2 Coherence** — any two aliases of a cell observe the same current target.
- **I3 Target neutrality** — `set` mutates only the cell's slot, never the old
  or new target.

## Rebind vs mutate (why the box is needed)

In Python/JS/Ruby every variable already holds a reference, but two operations
look alike:

```js
c = t        // REBINDING: rebinds the name — invisible to other aliases
c[0] = t     // MUTATION:  mutates the shared object — visible to every alias
```

The cell turns re-pointing from a *rebind* into a *mutate*, which is the only
thing aliases can observe.

## Files

| file | language | shows |
|------|----------|-------|
| `js/cell.js` | JavaScript | the primitive + I1/I2/I3 proof |
| `js/refcounted_cell.js` | JavaScript | cell + manual refcount (re-point → reclaim) + cycle caveat |
| `nim/cell.nim` | Nim | the primitive + I1/I2/I3 proof |
| `nim/refcounted_cell.nim` | Nim | deterministic reclamation via ARC/ORC `=destroy` |

## Run

```bash
# JavaScript
node stable_cell/js/cell.js
node stable_cell/js/refcounted_cell.js

# Nim (compiled; deterministic reclamation under arc/orc)
nim c -r --hints:off --outdir:/tmp/nimcell stable_cell/nim/cell.nim
nim c -r --hints:off --mm:arc --outdir:/tmp/nimcell stable_cell/nim/refcounted_cell.nim
nim c -r --hints:off --mm:orc --outdir:/tmp/nimcell stable_cell/nim/refcounted_cell.nim
```

## Scope

- **In scope:** the stable cell (axis 2: mutability/rewiring) + reclamation
  composed on top via refcount/ARC — acyclic, single-threaded.
- **Postponed:** cycle collection; concurrency (deferred/hazard reclamation).

See [`docs/CONVERSATION.md`](../docs/CONVERSATION.md) for the full conceptual background.
