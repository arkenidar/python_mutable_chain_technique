# Copy-Jump VM — compile-time reclamation over a bounded machine

A minimal **copy / jump / peek / poke** machine over a bounded address space,
built to make the theory in [`../docs/CONVERSATION.md`](../docs/CONVERSATION.md)
(§17–22) runnable: finite control + bounded memory → **liveness is decidable**,
so reclamation can be proven *exactly* at compile time.

## What it is

- **copy** — move a value between cells.
- **jump** — (conditional) branch.
- **peek** — read a cell (a *use*).
- **poke** — write a cell (an *allocation*).

Because the address space is finite (`N` cells × `V` values = `V^N` states), the
machine has finitely many configurations, so halting/liveness is decidable by
exhaustive reachability (cycle detection) — no approximation, no GC, no refcount.

## The reclamation rule

- `poke` = allocate/initialize a cell,
- `peek` / `copy`-src = use a cell,
- a cell is **dead** once no future instruction can read it,
- **reclamation = proving that point and overwriting/reusing the cell**.

## Structure

- [`src/vm.py`](src/vm.py) — the machine + exhaustive reachability + liveness (Python).
- [`src/vm.nim`](src/vm.nim) — the same machine + analysis (Nim).
- [`docs/DESIGN.md`](docs/DESIGN.md) — the design rationale and corollaries.

## Run

```bash
python3 copy_jump_vm/src/vm.py
nim c -r --hints:off --outdir:/tmp/nimcell copy_jump_vm/src/vm.nim
```

## Corollaries (why this exists)

See [`docs/DESIGN.md`](docs/DESIGN.md) and `../docs/CONVERSATION.md` §17–22 for:

- finite control + unbounded memory = Turing-complete → liveness undecidable;
- bounded memory = finite configurations → liveness decidable (cycle detection);
- state space `V^N` = `2^(V·2^A)` for A address bits — address range matters
  double-exponentially, value range only linearly;
- below the enumerability threshold, reclamation is **exact and exhaustive**,
  not conservative.
