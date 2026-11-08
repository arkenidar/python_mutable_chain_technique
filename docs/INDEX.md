# Index — reading guide

This repository is the **theory** half of a two-repo research program; the **research**
half (the actual RESM/BBJJ machine, its compilers, and its programs) lives in the
sibling repo `copyjumpvm/` (`../../copyjumpvm/`).

## Start here

1. **The synthesis** — [`THESIS.md`](THESIS.md): the canonical, distilled statement of
the whole idea (origin → machine → ABI → cell / verifiability / data-relations →
reduction → seed / colony / stream → Kolmogorov benchmark → novelty). Read this first.
2. **The discourse** — [`CONVERSATION.md`](CONVERSATION.md): the raw log of how we got
here — every idea, correction, and open question, in order. Archive / detail, not a
linear read.
3. **The record** — [`RECORD.md`](RECORD.md): plan, steps, issues, knowns vs unknowns.
4. **The structure** — [`STRUCTURE.md`](STRUCTURE.md): how this is organized and how it
grows.

## Code

- [`../stable_cell/`](../stable_cell/) — the stable cell primitive (`[t]` / `c[0]` /
  `c[0] = t`), JS + Nim.
- [`../copy_jump_vm/`](../copy_jump_vm/) — the RESM/BBJJ machine (`bbjj.py`), the
  fixed-input superoptimizer (`superoptimize.py`), and the verified peephole reducer
  (`bbjj_reduce.py`).

## Where next (the research repo)

- `../../copyjumpvm/docs/research_roadmap.md` — the six research directions.
- `../../copyjumpvm/docs/resm_bbjj.md`, `reuse_graphs_paths.md` — machine + reuse details.
