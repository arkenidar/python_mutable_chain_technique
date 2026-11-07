# Stable Cell — Conceptual Notes (conversation documentation)

This document records the conceptual arc behind the code in this directory: the
technique itself, and how it relates to garbage collection, reactive updates,
and memory/resource allocation and reclamation.

## 1. Origin: the mutable chain technique

The starting point was the repo's `mutable_chain.*` — a pattern for safe
deletion during iteration of a linked structure. Instead of storing direct
references (`node["next"] = other`), it wraps them in a mutable container
(`node["next"] = [other]`) and rewires links by mutating the container's
*contents*, never its *identity*. Deleting B from A→B→C is two content updates:
`A.next[0] = C` and `C.prev[0] = A` — the same container objects, new contents.

## 2. The stable cell: the primitive underneath

Extracted general primitive:

    make(target) -> cell    get(cell) -> target    set(cell, t) -> void

Invariants:

- **I1 identity stability** — `set` never changes the cell's identity.
- **I2 coherence** — all aliases observe the same current target.
- **I3 target neutrality** — `set` mutates only the slot, never the targets.

In dynamic languages the cell is a one-element list/array: `[t]` / `c[0]` /
`c[0] = t`.

## 3. Rebind vs mutate (the key distinction)

In Python/JS/Ruby every variable already holds a reference, so why the box?
Because there are two *different* operations:

    c = t      # REBINDING: rebinds the name — invisible to other aliases
    c[0] = t   # MUTATION:  mutates the shared object — visible to all aliases

The cell turns re-pointing from a *rebind* into a *mutate*, the only thing
aliases can observe.

## 4. Two orthogonal axes

- **Axis 1 — lifetime/ownership:** when is a node safe to reclaim?
  (scope → heap/reachability → refcount → tracing → deferred/concurrent)
- **Axis 2 — mutability/rewiring:** can an edge be redirected while observers
  keep a stable reference? (immutable → stable cell → reactive notification)

The cell lives on axis 2 and is agnostic to axis 1 — that's why scope, arena,
`shared_ptr`, or GC can all be plugged in beneath it.

## 5. Memory models and the scope-based (simple) case

An object's lifecycle has three stages: live/reachable → eligible/unreachable →
reclaimed. "Out of scope" grants *eligibility*, not reclamation.

| Model | scope exit → deallocate? | trigger |
|---|---|---|
| automatic/stack (C) | yes, synchronous | frame pop |
| RAII (C++, Rust) | yes | destructor at scope exit |
| refcount (`shared_ptr`, ARC) | conditional | decrement; free iff zero |
| tracing GC (JS/Python/Lua/Ruby/…) | no | later sweep, pressure-driven |

Scope-based freeing only works for *nested (tree) lifetimes*. The mutable
chain's lifetimes are *graph/aliased* — a node is created in one scope, unlinked
in another, still walked by an iterator in a third — which is exactly when scope
discipline breaks and you need refcount/GC/arena.

## 6. Reactive updates

A stable cell *is* a single-slot signal: observers subscribe to the cell (a
fixed identity), not to the value, so they don't re-subscribe when the value
changes. Same shape as Solid's `createSignal`, MobX's `observable.box`, Vue's
`ref`, RxJS `BehaviorSubject`, Svelte stores, React `useState`/`useMemo`.

The cell supplies the *substrate* (identity + coherence) but not the *reaction*:
you must add a subscriber list, change notification, and dependency/ordering
handling (glitch avoidance). It supports both push (notify on set) and pull
(read + dirty flag).

## 7. Reactive memory reclamation

Reclamation must be deferred and event-driven, because eager free is exactly the
use-after-free the technique exists to avoid:

    unlink → retire/defer → reader-exit → reclaim

This is safe/deferred memory reclamation (SMR): epoch-based reclamation, hazard
pointers, RCU. The cell is literally the hazard/publication slot readers write
into and reclaimers react to. Reference counting is its simplest form: the
"refcount hits zero" event is the free event (RAII destructor).

Pull-based flavor: weak-reference memoization (reclaim on pressure, recompute on
access) and free lists.

## 8. Memory- vs resource-allocation

- **Memory allocation:** the cell *costs* — one extra heap cell + indirection
  per edge, worse locality, GC pressure, and refcount cycles. Exception: it
  enables arena/region allocation and stable handles (movable/swappable
  objects).
- **Resource allocation:** the cell *is* the handle/indirection-table pattern —
  a Unix fd is a stable slot whose contents `dup2` can change, and every holder
  sees the new file; same for Windows HANDLEs and GPU/hot-reload slots.

## 9. Generality ladder (relax one assumption at a time)

| relax | new shape | minimal mechanism |
|---|---|---|
| (none — the simple snippet) | single ref, nested scope | stack / RAII |
| value escapes | lifetime outlives scope | heap |
| aliasing | DAG | reference counting |
| cycles | cyclic graph | tracing GC / weak / arena |
| re-pointing | mutable edges | **stable cell** |
| concurrency | readers vs writers | deferred reclamation |

## 10. Reference-graph abstraction

nodes = objects; edges = references; a cell = a redirectable edge; reclamation
= deleting a node once unreachable from the roots. Scope is the smallest region;
an arena is a manual region; generational GC is regions by age.

## 11. Real languages that already do this

| our artifact | real mechanism | languages |
|---|---|---|
| `cell.js` `[t]` | reference cell | Python/JS/Ruby/Lua idiom; first-class: OCaml `ref`, Rust `Cell`/`RefCell`, Clojure `Atom`/`Ref`, Haskell `IORef` |
| `refcounted_cell.js` | deterministic refcount on rebind | CPython, Swift/Obj-C ARC, Perl, Tcl, Squirrel |
| (postponed) cycles | cycle collector over refcount | CPython `gc`, PHP, Squirrel, Nim `refc`; or tracing GC (Lua, JS, Ruby, Wren, Io) |
| (postponed) concurrency | deferred / SMR | Rust `crossbeam-epoch`/`seize`/`haphazard`, C++26 `std::hazard_pointer` |

Simple + JS-like for study: Lua (mark-sweep; table as the universal cell), Wren
(mark-sweep), Squirrel (refcount + cycle collector), Io (prototype-based,
incremental mark-sweep).

## 12. Nim in detail

Nim selects its memory model at compile time (`--mm:`): `orc` (ARC + cycle
collector, default since 2.0), `arc` (plain ARC, no cycles), `refc` (deferred
refcount + cycle collector), `atomicArc` (atomic refcount), `markAndSweep`,
`boehm`, `none`.

- **Stable cell:** a single-field `ref object`; `let` freezes the ref, not the
  pointee — so `cell.value = t` is identity-stable and coherent (I1/I2/I3).
- **Re-point → reclaim:** under `arc`/`orc` the compiler inserts retain/release,
  so `cell.value = newTarget` releases the old target; at count zero the
  `=destroy` hook fires deterministically (the free event).
- **Cycles:** `orc`/`refc` collect them; `arc` does not (leaks cycles).
- **Concurrency:** `atomicArc` + `isolated` ownership (no built-in EBR/hazard).
- **Ownership:** `sink`/`lent`/`owned` add move/borrow semantics.

## 13. Postponed topics

- **Cycles** — collected by CPython `gc`, PHP, Squirrel, Nim `orc`/`refc`, or any
  tracing GC. Naive refcount (and `--mm:arc`) leaks them.
- **Concurrency** — deferred reclamation lives in Rust crates and C++26; not in
  Nim or the dynamic languages' runtimes as a first-class "hazard slot".

## 14. Formal anchors

- **Escape analysis** decides "scope model or heap model".
- **Region inference** (Tofte–Talpin) groups allocations into regions.
- **Ownership / borrowing / lifetimes** (Rust) generalize scope to lifetime regions.
- **Tracing and refcounting are duals** (Bacon–Cheng–Rajan 2004).

## 15. Practical aspects — for the end-user programmer

**The `[t]` box is a real daily tool.** In Python/JS/Ruby/Lua the one-element
list/array/table is idiomatic for: mutable state captured by a closure (Python
pre-`nonlocal`), out-parameters, and shared state between callbacks. Use it when
you need *shared, re-pointable* state that plain assignment can't express. Don't
reach for it when a plain variable suffices (single owner, no re-pointing), and
prefer a named object (`{v: x}`) or a small class over a magic `[0]` for code you
must maintain.

**"Out of scope" ≠ "freed".** In a GC language a variable leaving scope only
makes its object *eligible*; it is not reclaimed immediately. For files, sockets,
locks, and DB handles use deterministic cleanup — `try/finally`, `with` (Python),
`using` (C#/JS), or an explicit `.close()` — never scope exit. JS has no reliable
finalizers at all, so this is mandatory there.

**Refcount vs. GC changes what you can rely on.** CPython is *mostly*
deterministic: rebinding/`del` frees the instant the count hits zero (RAII-like),
*except* cyclic garbage, which waits for `gc.collect()`. JS/Ruby/Lua are
non-deterministic — never rely on timing or `__del__`-style finalizers.

**Cycles are the practical killer.** Real graphs (DOM, parent↔child, doubly
linked lists, caches, observer lists) are full of cycles, and the language's
answer determines your daily burden: JS/Java/Lua/Ruby collect cycles for you;
Swift/Obj-C ARC makes you mark back-references `weak`/`unowned` or leak; Python
collects cycles but finalizers on cyclic objects are unreliable.

**Reactivity: use the ecosystem's cells, don't hand-roll them.** If you need
"run when this changes," use Vue `ref`/`computed`, Solid `createSignal`, Svelte
stores, or MobX `observable` — the same primitive plus the notification and
scheduling layer you should not reinvent.

## 16. Practical aspects — for the language-designing programmer

**Reclamation is a trade-off spectrum, not a winner.** Refcount (deterministic;
"re-point → reclaim" is free and immediate; costs inc/dec everywhere, needs
atomics for threads, needs a cycle collector) vs. tracing GC (simplest for the
user, handles cycles and concurrency, but non-deterministic and unfriendly to
finalizers and FFI) vs. arena/region (fastest for bulk lifetimes, best locality,
but forces phased allocation) vs. manual (total control, total responsibility).
Nim's answer — expose the choice as a `--mm:` flag — is legitimate design.

**The cell is cheap to provide and expensive to get wrong.** *Free version:* any
language with mutable containers already has the `[t]` cell — no feature needed.
*First-class version:* OCaml `ref`, Rust `Cell`/`RefCell`, Clojure `Atom` give it
syntax and (in Rust) compile-time aliasing safety. *Design detail:* positional
slot (`c[0]`, magic index) vs. named slot (`c.value`, self-documenting) — prefer
named, or provide sugar.

**Design the two axes separately, then compose.** Axis 2 (rewiring) = cell /
handle; axis 1 (lifetime) = refcount/tracing/arena. Good languages pick one of
each and keep them orthogonal: Rust (`Cell`/`RefCell` + ownership), Swift (class
refs + ARC/`weak`), Nim (`ref` + `--mm:`), Clojure (`Atom` + JVM GC).

**Deterministic destructors vs. finalizers.** Separate *memory reclamation* (may
be non-deterministic) from *resource finalization* (must be deterministic). RAII
languages (C++/Rust/Nim ARC) unify them; GC languages compensate with
`IDisposable`/`using`, context managers, `try-with-resources`. A cycle that
prevents a destructor from ever running is a real footgun in RAII+refcount.

**Reactivity as a language/VM concern.** If reactivity is first-class (Solid/
Svelte compile-time signals, or a runtime "reactive variable"), the cell must
carry a subscriber list and change notification, and you must solve dependency
tracking, glitch avoidance (topological scheduling), and subscription disposal.
Otherwise, keep the cell dumb and leave reactivity to libraries.

**Concurrency/reclamation is a deep commitment.** Lock-free data structures force
a SMR choice (hazard pointers / EBR / RCU) or a global GC; scripting languages
usually sidestep this with a GC (often plus a GIL). You can't bolt this on later
— it shapes the whole runtime.

**A design checklist** (one line per axis): default reclamation? opt-outs
(manual/arena/`weak`)? first-class cell or container idiom? cycles automatic or
delegated? destructors deterministic or best-effort? reactivity built-in or
library? concurrency via GC/GIL or SMR/atomics?

## 17. Designing a compile-time reclamation system

Goal: reclamation (including cycles and concurrency) by *reliable rules* whose
data scopes are precise enough to be **calculated at compile time**.

This is the **region + ownership + borrowing** family (Cyclone regions, ML Kit
region inference, Rust ownership, Pony reference capabilities, Vale regions,
Mezzo). The honest foundation: liveness is undecidable (Rice's theorem), so every
such system is a **restricted core + escape hatches** — you gain no-GC, no-leak,
deterministic destruction; you lose some expressiveness.

**The rules:**

1. **Region rule** — every value belongs to one *region* (a group with a
   compile-time lifetime); a region is freed LIFO at scope exit. Reclamation
   *is* region-free — placed by the compiler.
2. **Ownership rule** — within a region, a value has one *owner*; moving
   transfers ownership and invalidates the old owner (linear/affine types).
3. **Borrowing rule** — a borrow is a non-owning reference whose lifetime is
   provably contained in the owner's region; borrows never reclaim.
4. **Cell rule** — the stable cell is a single-slot owner; `set(cell, t)`
   compiles to `drop(cell.slot); cell.slot = t` (release old, acquire new,
   both at compile time). Identity stable; contents owned.
5. **Cycle rule** — a cycle is owned by its *region*, not its nodes: cyclic
   graphs live in an arena whose intra-region links are borrows, and the whole
   region is freed at once (no collector, no `weak` within the region).
6. **Concurrency rule** — a value crosses threads only if ownership proves
   safety: move a unique owner; share only immutable (`Sync`) or locked
   (`Mutex`/`RefCell`) values; borrow only into *scoped* threads whose lifetime
   is ≤ the borrow's region.

**Central tension:** compile-time reclamation and *persistent* shared aliasing
conflict. The stable cell's full power is had either as scoped borrows (static,
transient) or via `Rc`/`Arc` (runtime refcount — the one place runtime can't be
fully escaped). Fully static = unique owner + borrows + region-grouped cycles.

**Result:** no use-after-free, no double-free, no leaks, no data races — checked
at compile time; destruction deterministic, placed by the compiler.

Concrete sketch (Rust-like):

```rust
let x = String::from("hi");
let y = x;                       // move: x invalid now
let r: &String = &y;             // borrow, lifetime <= y's region
let cell = Cell::new(y);         // cell owns y
cell.set(String::from("bye"));   // drop old, take new — compile-time
{
    let arena = Arena::new();    // region owns the cycle
    let a = arena.alloc(Node);
    let b = arena.alloc(Node);
    a.next = b; b.next = a;      // cycle; reclaimed with the arena
}
std::thread::scope(|s| s.spawn(|| use(&y)));  // scoped borrow across a thread
```

Escape hatches: `unsafe` (Rust), `Rc`/`Arc`/`weak`, `RefCell`/`Mutex` (runtime
borrow/lock), region inference vs. explicit regions (ML Kit vs. Rust).

Reference designs: Rust (ownership + borrowing), Pony (reference capabilities +
actors), Vale (regions + generational references), Mezzo (permission inference).

## 18. Why the honest limit is fundamental

Liveness ("will this object ever be accessed again?") is what reclamation needs,
and it is **undecidable**: reduce the halting problem — for any program P,
`x = allocate(); P; use(x)` has `x` dead immediately iff `P` halts. By Rice's
theorem any non-trivial semantic property is undecidable. So a compile-time
system must be **sound but incomplete**: it accepts a *restricted core* where
lifetime is syntactically evident (regions, ownership, scoped borrows) and
rejects some valid programs — those go to escape hatches.

The escape hatches are **minimal, not failures** — each adds only the runtime its
situation demands:

| hatch | runtime added | situation admitted |
|---|---|---|
| `Rc`/`Arc` | reference count | persistent shared ownership |
| `weak` | liveness check | observe reclamation / break cycles |
| `RefCell`/`Mutex` | borrow/lock flag | aliasing **and** mutation |
| `unsafe` | none (trust the human) | raw pointers, FFI, self-reference |

**Why persistent shared aliasing is the irreducible case:** a static system can
track a *statically-known* set of references (tree owner + scoped borrows), but
persistent aliasing means the owner set grows/shrinks at runtime, so "when the
last owner is gone" is a runtime event → refcount or GC. This is the
aliasing-XOR-mutation trilemma: shared refs forbid mutation statically; mutable
refs forbid sharing statically; both together require runtime.

The stable cell's coherence across aliases (I2) *is* persistent shared aliasing,
so the cell is the one construct that cannot be made fully static — its static
approximations are scoped borrows (transient) or unique ownership (no sharing).

The boundary is real but malleable: region inference (ML Kit), fractional
permissions (separation logic), and reference capabilities (Pony) enlarge the
static core; generational references (Vale, `slotmap`, handle tables) shrink the
residual runtime to a single integer comparison instead of a full refcount/GC.
It cannot be eliminated for arbitrary persistent sharing.

## 19. The finite-tape refinement

The undecidability of liveness holds for *unbounded* memory. With a bounded tape
of n cells, a machine has finitely many configurations (|Q|·|Γ|^n·n), so halting
is **decidable**: simulate and detect a repeated configuration (a cycle). This is
the linear-bounded-automaton case — halting/acceptance on a *given* input is
decidable, though LBA *emptiness* (over all inputs) is undecidable (Kuroda 1964).

**Decidable ≠ feasible:** a real computer with M bits has ≤ 2^M states — a finite
automaton in principle, but its state space is astronomically larger than the
observable universe, so cycle detection is never actually run.

**Undecidability returns when the bound grows:** fix any tape bound → decidable;
let it grow with the input → undecidable. The two statements are consistent.

**Implication for the design:** the "restricted core" is the practical way to
exploit this — regions/ownership/borrows make the finite-state argument hold
*by construction* without exploring the state space. Explicitly bounding memory
(regions/arenas with bounded sizes) makes liveness decidable in principle; the
syntax makes it decidable *and* cheap.

## 20. The copy-jump-peek/poke machine

A minimal machine with `copy` (move data), `jump` (branch), `peek` (read memory),
and `poke` (write memory) is **Turing-complete**, and — like every Turing machine
— it is a **finite object** (finite instruction set, finite state, finite program)
operating on **unbounded memory**. The finiteness is in the control; the infinity
is in the address space that peek/poke touch. (Same family: x86 `mov` is
Turing-complete, `subleq`, Minsky two-counter machines.)

**Consequence:** undecidability lives entirely in the unbounded memory, never in
the finite control. Bound the peek/poke address space → finitely many
configurations → halting/liveness decidable (section 19). Unbounded → undecidable.

**Why it's a good substrate for compile-time reclamation:** peek/poke make *every*
memory access explicit and statically visible — the compiler sees exactly where a
region is first written (`poke` = allocate/init) and last read (`peek` = last use).
Lifetime analysis reduces to: a region is reclaimable when no future `peek` can
address it, so the compiler pops the region at that point — no GC, no refcount.
Regions = address ranges; copy/jump = data movement and control flow; the
ownership/borrowing rules (section 17) become static facts about peek/poke.

**Caveat:** the finite instruction set does not remove the hard part — the program
can still poke unbounded addresses, so liveness stays undecidable unless memory is
bounded or region-structured. The machine just makes the hard part *visible and
localized* to the peek/poke operations, which is exactly the leverage needed.

## 21. Limiting address and value ranges (0-255)

Limit the peek/poke address space to 256 cells and each cell to a byte (0-255):
memory has 256^256 = 2^2048 possible contents, times the finite control state — a
**finite** configuration space, so halting/liveness is decidable in principle
(cycle detection). But 2^2048 is astronomically infeasible (the observable
universe is ~2^266 atoms), so "decidable" ≠ "practically computable".

**The deeper point:** at 256 bytes you don't need reclamation at all — you can
pre-allocate the whole address space (static allocation). Reclamation is a problem
of *large/growing* memory, not memory per se; that's why tiny machines (Atari
2600's 128 B, early microcontrollers) had no heap/GC. **Address range** is the
crucial parameter (it is the "tape size"); **value range** only shrinks the
already-finite constant.

**Spectrum:** tiny fixed memory → no reclamation (pre-allocate); small fixed →
static partition or simple regions; large/unbounded → reclamation + compile-time
rules (regions/ownership) to avoid enumerating the state space.

A 256-byte machine is small enough to *build* a real compile-time lifetime
analysis over (abstract interpretation or even exhaustive exploration), with
`poke` = write/allocate a region, `peek` = read/use, and reclamation = proving no
future `peek` reaches a region, then popping it — all provable at compile time.

## 22. A 4-bit nibble (and the enumerability threshold)

Memory states = 2^(V·2^A), where A = address bits and V = value bits. A appears
double-exponentially (2^A) and V only linearly — so shrink the *address* range,
not just the value range.

- A=8,V=8 (256 B): 2^2048 — infeasible.
- A=4,V=4 (nibble-nibble): 2^64 — borderline (symbolic only).
- A=3,V=3: 2^24 = 16.7M — **directly enumerable in milliseconds**.
- A=2,V=2: 2^8 = 256 — trivial.

**Threshold:** direct exhaustive search works to ~2^30–2^32 states; symbolic
methods (BDDs, SAT-based model checking) to ~2^100+ for structured systems.
Below the threshold, the compiler can decide liveness *exactly and exhaustively*
at compile time — enumerate every reachable configuration and ask whether a cell
is ever peeked again. No conservative rules, no escape hatches, no GC, no
refcount.

**Trade-off:** a nibble machine is too small for general programs, so exact
reclamation trades away expressiveness. The design axis is **machine size ↔
(exactness of reclamation vs. expressiveness)**. The "restricted core + escape
hatches" is the large-machine compromise; the nibble is the small-machine extreme
that can afford full state search. (4-bit machines are real: Intel 4004.)

## 23. Composable nibble machines: expressivity vs knowability

Fixed composition of finite machines = a finite machine (product of states), so it
is **not** Turing-complete and its state space grows `2^(64k)` for k nibble
machines — knowability (exact enumeration) dies fast. Unbounded composition
(recursion/loops that grow memory) is Turing-complete but reintroduces
undecidability. So **full expressivity + full knowability is impossible**
(theorem: Turing ⟺ unbounded state ⟺ undecidable).

The working middle ground is **hierarchical composition with finite I/O
interfaces** (hardware verification's assume-guarantee): each nibble machine is a
finite transducer over small ports; you verify it exactly and compose
*summaries*, never the product state space. This gives scalable expressivity +
local knowability.

Two principled endpoints:
- **Total languages** (Coq/Agda/Idris/Lean): every program terminates, so
  "knowable" completely, but not Turing-complete (only well-founded recursion).
- **Turing-complete + approximate verification**: abstract interpretation (sound
  over-approx), bounded model checking (exact to K steps), theorem proving.

"Knowability" has a cost ladder, each reducing to halting: termination →
liveness/reclamation → full correctness (strictly harder).

Design axis: "total/bounded" (knowable, less expressive) ↔ "unbounded +
approximate" (expressive, partly knowable); composable nibble machines sit in the
productive middle.

## 24. Boundedness as the design principle (unboundedness is out of scope)

"Unboundedness" is a *conditional marker*, not a leak: undecidability and Turing
completeness enter **only if** the design lets the bound grow. A fixed-bound
design rules them out by construction, so for it they are moot.

- bounded, small (nibble) -> exact + enumerable (trivially knowable);
- bounded, large -> decidable in principle; keep knowability *practical* via
  compositional methods (hierarchical nibble machines + finite summaries);
- unbounded -> undecidable (not built here).

Commit to fixed bounds => exact knowability; the only thing given up is unbounded
input sizes (which is what Turing-completeness requires anyway). The impossibility
theorem only concerns *unbounded* systems, so a bounded design resolves the
tension in favor of knowability - a feature, not a limitation.

## Chronological summary

1. Studied `mutable_chain.*` (7 ports) — the list-wrapper indirection pattern.
2. "Is it useful to avoid GC?" — No: orthogonal; the C++ `Ref<T>` leaks via
   `shared_ptr` cycles.
3. "Reactive updates / allocation?" — signal primitive; allocation cost with an
   arena exception; resource = handle/indirection.
4. "Reactive reclamation?" — deferred/event-driven (EBR / hazard pointers / RCU).
5. Scope-based snippet → three-stage lifecycle; JS syntax ≠ JS behavior.
6. "Too simplistic?" → generality ladder + two axes + reference graph.
7. Focused on mutable edges → stable cell spec + three re-pointing strategies.
8. Focused on `[t]`/`c[0]`/`c[0]=t` → rebind vs mutate.
9. Built `stable_cell/js/cell.js` + `stable_cell/js/refcounted_cell.js`; moved to `stable_cell/js/`.
10. Postponed cycles and concurrency.
11. Mapped the mechanisms to real languages (CPython, Swift, Lua, Wren, Nim…).
12. Nim deep dive → built `stable_cell/nim/cell.nim` + `stable_cell/nim/refcounted_cell.nim`.
