# Stable Cell Project — Full Record

A complete record of the exploration: plan, steps, dialogues, rationales,
issues, knowns vs. unknowns. Companion to `README.md` (practical) and
`CONVERSATION.md` (conceptual, sections 1–22).

## 1. Plan & intent

- **Original goal:** study the repo's "mutable chain" technique.
- **Evolution:** the study surfaced a primitive (the **stable cell**) and opened
  a wider investigation: GC vs. refcount vs. arena, reactive updates,
  memory/resource allocation, reactive (event-driven) reclamation, scope-based
  deallocation, a "generality ladder," real-language mapping, Nim's selectable
  MM, and finally a design for **compile-time reclamation** (regions +
  ownership + borrowing), bounded by the fundamental limits (undecidability for
  unbounded memory; decidability-in-principle for bounded memory).
- **Code produced:** JS + Nim implementations of the stable cell and a
  refcounted variant, plus documentation.

## 2. Steps / timeline

1. Read the repo (`mutable_chain.{py,c,cpp,lua,js,mjs,rb}`, README, BUILD,
   CMake); built and ran all 7 ports (Python, Node×2, Ruby, LuaJIT, C, C++).
2. Identified the technique (wrapper indirection for safe deletion during
   iteration) and four issues (see §5).
3. Conceptual discussion: GC usefulness, reactivity, reclamation, scope,
   generality (CONVERSATION.md §2–9).
4. Built `stable_cell/js/cell.js` + `stable_cell/js/refcounted_cell.js`.
5. Moved them into `stable_cell/js/`.
6. Built `stable_cell/nim/cell.nim` + `stable_cell/nim/refcounted_cell.nim` (fixed the Nim
   `=destroy` signature).
7. Wrote `stable_cell/README.md` + `docs/CONVERSATION.md`; linked from the main README.
8. Added practical (§15–16) and design/fundamental (§17–22) sections.
9. This document (`RECORD.md`).

## 3. Dialogues (prompt → outcome)

1. "study this technique (chain)" → read + ran all ports; found the primitive
   and its subtleties (§1).
2. "useful to not rely on GC?" → no: orthogonal; C++ `Ref<T>` leaks via cycles.
3. "good for reactive-updates / allocation?" → signal primitive; memory cost +
   arena exception; resource = handle/indirection (§6–8).
4. "reactive memory-reclamation?" → deferred/event-driven (EBR/hazard/RCU) (§7).
5. [scope snippet] "de-allocated after out-of-scope" → three-stage lifecycle;
   JS syntax ≠ JS behavior (§5).
6. "too simplistic? reach generality" → generality ladder + two axes +
   reference graph (§9–10).
7. "only re-pointed → mutable edges → stable cell" → Cell spec + three
   re-pointing strategies (§2–3).
8. "Python/JS/Ruby [t] c[0] c[0]=t" → rebind vs mutate (§3).
9. "…for JS" → built the JS cell + refcounted cell.
10. "move to /cell/js/" → done.
11. "cycles and concurrency postponed" → scoped the work.
12. "simple js-like languages with these mechanisms?" → CPython/Swift/Lua/Wren/
    Squirrel/Io/Nim/OCaml/Rust/Clojure (§11).
13. "tell me about Nim" → Nim MM strategies, cell in Nim, ARC/ORC, sink/lent
    (§12).
14–15. "yes" / "continue" → built the Nim cell + refcounted cell.
16. "document this and all the conversation" → README + CONVERSATION + main
    README link.
17. "practical aspects (end-user vs language designer)" → §15–16.
18. "design a system … reclamation by reliable rules … compile time" → §17.
19. "the honest limit … liveness undecidable" → §18.
20. "halting problem solved for non-infinite tapes" → §19.
21. "copy-jump-peek/poke machine … finite properties" → §20.
22. "limit address/value ranges to 0-255?" → §21.
23. "or even a 4-bit nibble" → §22 (state formula + enumerability threshold).
24. "document all these said things" → this RECORD.md.

## 4. Rationales & explanations (the "why")

- **Why the box (`[t]`) exists:** in dynamic languages every variable is already
  a reference, but `c = t` *rebinds* the name (invisible to aliases) while
  `c[0] = t` *mutates* the shared object (visible to all). The cell turns
  re-pointing into mutation.
- **Why it's orthogonal to GC:** the cell manages *identity/aliasing* (axis 2),
  not *lifetime* (axis 1). Reclamation must be layered separately.
- **Why liveness is undecidable:** reduces to halting (`x = alloc(); P; use(x)`
  has `x` dead iff `P` halts); Rice's theorem.
- **Why bounded memory is decidable-in-principle:** finite tape → finite
  configurations → cycle detection. But `2^(V·2^A)` states → infeasible.
- **Why address range dominates:** `A` is double-exponential (`2^A`), `V` linear.
- **Why persistent shared aliasing is irreducible at runtime:** a static system
  tracks a fixed set of refs (tree + borrows); a dynamic owner set needs
  refcount/GC. Aliasing-XOR-mutation trilemma.
- **Why the cell = a signal:** observers subscribe to the cell's fixed identity,
  not the value.

## 5. Criticities & issues

- **C++ `MutableList` cycle leak:** `Ref<T>::ptr` is `shared_ptr` for both
  `next`/`prev` → `node→Ref→node` cycles; refcount never reaches 0 → leak.
- **C++ `erase()` footgun:** `erase(it)` returns the successor, but the demo
  discards it and does `++it`; mixing `it = erase(it)` with `++it` skips the
  successor.
- **Docstring imprecision:** the forward iterator continues via the deleted
  node's *own* intact outgoing cell; the cell rewrite keeps *other* observers
  (and reverse traversal) coherent — the README credits the wrong mechanism.
- **C manual memory:** `link_data_remove` orphans without freeing; the demo
  relies on external `free_node(data1)`; a lost pointer leaks.
- **Nim `=destroy` signature:** must be on the object type (`var PayloadObj`),
  not the `ref` (`Payload`); initial version failed to compile, then fixed.
- **Decidable ≠ feasible:** `2^2048` (256 B) and even `2^64` (nibble) are beyond
  direct search; practical threshold ~`2^30` (direct) / ~`2^100` (symbolic).
- **Exact vs. conservative tension:** small machine → exact exhaustive
  reclamation but tiny programs; large machine → expressive but conservative.

## 6. Knowns vs unknowns

| Knowns (established) | Unknowns / open |
|---|---|
| Stable cell spec + I1/I2/I3 | cycle collection — documented, not implemented |
| rebind vs mutate | concurrency / deferred reclamation — postponed |
| two orthogonal axes (lifetime / rewiring) | the copy-jump-peek/poke VM — designed, not built |
| four memory models + 3-stage lifecycle | can BDDs scale exact reclamation to real sizes? |
| re-point → reclaim = CPython/Swift ARC/Nim ARC | the exact feasible threshold for "exact" reclamation |
| cycles: orc/refc/tracing collect; arc/naive leak | a cleaner static encoding of persistent aliasing? |
| compile-time reclamation = regions+ownership+borrowing | |
| liveness: undecidable (unbounded) / decidable (bounded) | |
| state formula `2^(V·2^A)` + enumerability threshold | |
| real-language mapping (CPython…Nim…Clojure) | |

## 7. Artifacts

```
docs/                    # narrative + process record
├── CONVERSATION.md      # conceptual: sections 1–22 + chronological summary
└── RECORD.md            # this file
stable_cell/             # the primitive (JS + Nim)
├── README.md            # practical: spec, invariants, files, run
├── js/
│   ├── cell.js
│   └── refcounted_cell.js
└── nim/
    ├── cell.nim
    └── refcounted_cell.nim
copy_jump_vm/            # compile-time reclamation experiment
```

Main `README.md`: indexes the whole structure.

## 8. Run

```bash
node stable_cell/js/cell.js
node stable_cell/js/refcounted_cell.js
nim c -r --hints:off --outdir:/tmp/nimcell stable_cell/nim/cell.nim
nim c -r --hints:off --mm:arc --outdir:/tmp/nimcell stable_cell/nim/refcounted_cell.nim
nim c -r --hints:off --mm:orc --outdir:/tmp/nimcell stable_cell/nim/refcounted_cell.nim
python3 copy_jump_vm/src/vm.py
```

