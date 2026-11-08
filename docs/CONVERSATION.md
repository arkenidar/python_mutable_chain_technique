# Stable Cell — Conceptual Notes (conversation documentation)

> This is the **raw discourse log** — the full, growing record of the conversation.
> The distilled, stable synthesis is in [`THESIS.md`](THESIS.md); see
> [`INDEX.md`](INDEX.md) for the reading guide.

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

> **Correction (§28):** the real machine is RESM/BBJJ (one instruction: copy a bit +
> 2-way jump), not this 4-op simplification; its arithmetic and loops are built in
> software. This section's point (finite control + unbounded memory → undecidability)
> still holds.

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

## 25. Restrictions needed for compositional knowability

Yes - composition must be restricted, and the restrictions are the two that make
compositional reasoning sound, plus finite interfaces:

1. **Acyclic / well-founded composition** (no unbounded feedback) -> preserves
   liveness knowability (termination, reclamation). A looping component is a
   fixed point, not a finite function, so it cannot be summarized; well-foundedness
   restores a finite summary.
2. **No shared mutable state across boundaries** (interface-only, no aliasing) ->
   preserves summarizability / modular verification. Shared mutable state means no
   closed summary exists.
3. **Finite bounded interfaces** -> preserves finiteness of summaries.

Property ladder: safety needs (2)+(3); liveness adds (1); full correctness adds
complete boundary specs (theorem proving).

Concretely: acyclic composition of finite transducers is always fully knowable;
feedback (cyclic) composition needs a termination bound to stay knowable.

These are the "restricted core" (boundedness + no-aliasing) restated at the
composition level; the stable cell (deliberate aliasing) is exactly the thing
needing the most care - shared mutable cells must be single-owned + borrowed or
immutable to stay compositional.

## 26. Recursive verifiable reduction (certified compilation / superoptimization)

Verification is recursive, so use recursion to *reduce* systems while preserving
behavior: a fragment is a black box with a **denotation** (observable I/O), which
is the invariant. A rewrite is valid iff it preserves the denotation
(behavioral equivalence / refinement). Apply rewrites bottom-up; by
compositionality the whole program's behavior is preserved.

Three ingredients (same restrictions, now doing work):
1. **well-founded rewrites** (size strictly decreases -> terminates at a normal
   form; confluence -> unique minimal form),
2. **closed black boxes** (no aliasing -> summarizable in isolation),
3. **compositionality** (reduce the parts => reduce the whole).

Levels: source -> IR -> byte-code; at byte-code level, peephole rewrites shrink
instruction sequences. For a copy-jump-peek/poke machine, program size =
instruction count = code memory, so reducing byte-code = reducing memory.

Nibble payoff: *verification* becomes exact - but only **fixed-input** exact.
"Equivalent on one given memory" is checkable by reachable-state enumeration
(cheap). "Equivalent for all 2^64 memories" is decidable but NOT enumerable in
practice - it needs SMT/symbolic methods (Souper/STOKE) or the test-vector
heuristic (Massalin). And the *search* over candidates is combinatorial
(~801^n instruction forms: length 1 ~ 801, length 2 ~ 642k, length 3 ~ 514M).
So superoptimization is "exact but combinatorial", not "free".

Honest limit: equivalence of arbitrary programs is undecidable. Two sound
strategies for the *search*: prove each rewrite rule once and reuse (CompCert /
equality saturation), or enumerate candidates and verify (Massalin / SMT). On a
bounded machine the *verification* is decidable (fixed-input by enumeration,
all-input by SMT), but the *search* is |I|^n - the real wall.

> **Correction (§28):** on the real machine (software arithmetic and loops), the
> shortest fixed-input program can use loops — a loop is far shorter than its
> unrolling — so straight-line BFS is optimal only for straight-line fragments; the
> general case is loop-aware (outlining / equality saturation / peephole).

## 27. Hutter Prize and compression as semantics-preserving reduction

Compression = approximate Kolmogorov complexity: the shortest (decompressor +
payload) that reproduces the data losslessly. It is §26's recursive reduction
applied to *data*, where the invariant is losslessness and the rewrites are
provably-invertible transforms.

The two superoptimization strategies map: "prove rules once" -> lossless
transforms (LZ/BWT/PPM/CM/arithmetic) for the *data*; "enumerate on a bounded
machine" -> the *decompressor program* (small), not the 1 GB payload (2^(8·10^9)
is non-enumerable).

Key simplification: a decompressor (the whole artifact: program + payload) has
**no variable input** - it is judged on one input and must produce one output.
So "equivalence" collapses to "outputs the same thing on that one input",
checkable by simply running it (fixed-input, exact). Verification was never the
hard part; the wall is *search*. This is Kolmogorov complexity's setup (an
inputless program): uncomputable on unbounded machines (halting sneaks into the
search), computable-in-principle on bounded machines (cycle detection decides
"halts and outputs x"), but still |I|^n combinatorial - and the machine must be
large enough to hold the output.

Hutter Prize pipeline: (1) probability modeling (context mixing / neural) - the
"intelligence"; (2) entropy coding (arithmetic / ANS) to the Shannon limit;
(3) preprocessing (BWT/MTF/dictionary); (4) minimize decompressor program size
(superoptimization fits here - the prize counts decompressor size); (5) massive
compute (ensembles).

Notable way: Shannon's source-coding theorem bounds compression by source entropy
under the best model; going lower requires better *prediction* = modeling the
generating process = Hutter's compression->AGI thesis. English text entropy ~1
bit/byte => ~8-10x is near the practical floor; current best (nncp/cmix) ~114 MB
on enwik9 (~8.8x), above the 100 MB target.

## 28. The real machine: RESM/BBJJ (correction of §20/§21/§26)

The actual copyjumpvm is **RESM** ("Raw Execution-Step Machine") aka **BBJJ**
("Bit Bit Jump Jump") — a single-instruction (OISC/URISC) machine, NOT the
16-cell poke/copy/peek machine modeled earlier. One instruction, four operands:

    [COPY_FROM, COPY_TO, IP_CASE_0, IP_CASE_1]
    1. fetch the bit at COPY_FROM
    2. write that bit to COPY_TO
    3. jump to IP_CASE_0 or IP_CASE_1, chosen by a path-selector bit

Textual form decomposes it: `m <dst> <src>` (copy one bit) and
`j <cond> <l0> <l1>` (2-way branch), plus `j <label>` (unconditional) and
`j <cond> <label>` (if 0 -> next else label). `@` is *dereference* (read ADDR_BITS
bits LSB-first as the address) = peek/poke/computed-goto — an addressing mode,
not an opcode. Bit memory (13-bit addresses ≈ 8 Kbit); bits 0 and 1 are constant
0/1 sources. **No ALU, no call stack**: arithmetic = gate circuits + ripple-carry
full adders; subroutine reuse = structural return / return register + dispatch /
a software stack (for recursion). Witnesses: MD5 (~376k instructions,
bit-identical to hashlib) and xorshift32.

Two corrections to earlier sections:
- §20 said "no ALU" and implied "no arithmetic/loops" — **wrong**: the ALU and
  loops are software-built, and that is how MD5/xorshift work.
- §26 implied "the shortest fixed-input program is straight-line" — **wrong**: a
  loop compactly expresses repeated work (a 32-bit ripple-carry add as a loop is
  far shorter than its unrolling), so jumps genuinely reduce size. Straight-line
  BFS (superoptimize.py) is optimal only for straight-line fragments; the general
  shortest program is loop-aware (loop synthesis is undecidable; practical tools =
  outlining / procedural abstraction, equality saturation, peephole).

What still holds: bounded memory -> decidable verification; fixed-input
verification = "run it once"; the search-vs-verification split; the composability
restrictions (§25). The repo's halting detection (iloop -> "infinite loop",
tloop -> "terminated") is §19-21 made concrete.

Practical smart size-reduction already in the repo: outlining / procedural
abstraction (outliner.js: integer_sum 233 -> 94 by sharing a repeated 29-instr
full-adder subgraph via return register + dispatch), peephole, and brute-force
synthesis (arkenidar/simple, for the decompressor direction).

## 29. The unified research program — from Hutter-prize compression to a bit-level minimal machine that unifies the stable cell, verifiability, and data-relations, and yields a reduction algorithm

> A self-contained synthesis of the whole arc (also standalone at `docs/THESIS.md`).

### 1. Origin — the Hutter Prize (compression as intelligence)

The Hutter Prize is a benchmark for lossless compression of a fixed 1 GB Wikipedia
snapshot, premised on the claim that **compression is a measurable proxy for
intelligence**: a self-contained program that regenerates a file *is* a model of
that file (it encodes its structure, hence "understands" it).

This research began (~20 years ago) from that thesis, in the **decompressor-generator**
direction — find a short program that, when run, reproduces the target data
bit-for-bit. That is Kolmogorov complexity in practice: K(x) = the shortest program
that outputs x.

The key pivot (recorded in the sibling repo's `decompressor-generator.md`) was to
stop funneling all effort into one statistic-dominated benchmark (context mixing +
arithmetic coding) and instead advance the **science of program optimization** on a
machine minimal enough that every finding is clean, reproducible, and transferable —
to compilers, microcode, circuit synthesis, and Kolmogorov-style complexity alike.

### 2. The machine — maximum flexibility at the minimum: RESM/BBJJ

The chosen substrate is **RESM** (*Raw Execution-Step Machine*) aka **BBJJ** (*Bit
Bit Jump Jump*): a single-instruction (OISC/URISC) machine.

```
[COPY_FROM, COPY_TO, IP_CASE_0, IP_CASE_1]
 1. fetch the bit at COPY_FROM
 2. write that bit to COPY_TO
 3. jump to IP_CASE_0 or IP_CASE_1, chosen by a path-selector bit
```

- **Bit-level**: memory is bit-addressable (13-bit addresses ≈ 8 Kbit); bits 0 and 1
  are constant 0/1 sources. The smallest grain of computation is *one bit*.
- **Small ISA**: a single instruction — **maximum flexibility** (Turing-complete) at
  **minimum richness**. It is the "zero point" of the ISA-richness curve: every
  convenience a conventional CPU puts in hardware is instead built in software.
- **No ALU, no call stack**: arithmetic (AND/OR/XOR/NOT, `+ mod 2^32`) is built from
  gate circuits and ripple-carry full adders (a gate ≈ 7 instructions, a full adder
  = 5 gates); subroutine reuse uses structural returns, a return register, or a
  software stack. Witnesses: **MD5** (~376k instructions, bit-identical to hashlib)
  and **xorshift32**, plus recursive `countdown`.

### 3. The added peek/poke — indirect addressing → pointers → C-style functions (an ABI)

An operand prefixed with `@` is **dereferenced**: the 13-bit value at that address
is used as the actual address. This is an *addressing mode*, not a new opcode, so
the machine stays single-instruction — but it gains **pointers**: indirect read =
`peek`, indirect write = `poke`, indirect jump = computed goto.

From pointers follow both **data structures** (arrays, the software stack) and —
decisively — **C-style functions with an ABI**. The "control track" (`cc.py`,
`callstack_gen.py`) builds exactly this tower:

```
Level 0  copy + jump + peek/poke              (bits)
Level 1  ABI: args in fixed registers, result in R0, return address on a
         software call stack, `j @reg` to return           (call/return)
Level 2  a C-like function language, compiled down      (sum, countdown, recursion)
```

The point: **the machine is a generic C-compiler target** — *without* an ALU and
*without* a hardware call stack, because both are software-implemented (gates and
full adders for arithmetic; the stack for calls). Recursion works because each call
pushes its own return address and the compiler saves/restores live arguments.

### 4. Toward data-reactivity and the stable-cell primitive

A peek/poke-addressable bit **is a stable cell**: its *identity* (the address) is
fixed while its *contents* (the bit) can be rewritten — the `[t]` / `c[0]` /
`c[0] = t` primitive (docs §2–3) at bit granularity. `peek` = get, `poke` = set.

- **Data-reactivity** (docs §6): the machine's state is a set of reactive cells; a
  "signal" is a cell plus change notification. On the minimal machine, the
  *rewiring axis* (the stable cell) and *memory access* are the same thing.
- **Two orthogonal axes** (docs §4): *lifetime/ownership* (when to reclaim a bit)
  vs *mutability/rewiring* (the cell). Both are visible at the bit level; the
  stable cell is the rewiring axis, and indirect addressing makes cells first-class.

### 5. Resurrecting program verifiability

Because memory is bounded, the machine has **finitely many configurations**, so
**liveness/halting is decidable** by cycle detection (docs §19–22) — the VM's
`iloop → "infinite loop"`, `tloop → "terminated"` is the concrete form.
Boundedness is thus a *design principle that buys knowability* (§24), and
compositional knowability needs exactly acyclic/well-founded structure + no shared
mutable state (§25). A bounded machine + fixed input makes **verification** cheap
("run it once"), leaving **search** as the hard part (§26–27).

### 6. Resurrecting data-relations

A program is **not an opaque I/O function** — it is a **dataflow graph of bit-level
relations**: a copy `m dst src` is an *identity* relation (`dst := src`); a gate is
a *boolean* relation (`out := f(in1, in2)`); a constant is a relation to bit 0 or 1.

So the **summary of a fragment** is its *derivation relation* — "these output bits
are defined by these expressions over the input bits" — the *intensional* structure,
not the extensional I/O table. This is what makes reduction factorable: sharing a
repeated *relation* (a gate, an adder) is the whole game.

### 7. Algorithm-making — a proper algorithm for reduction

**Goal**: the shortest equivalent bit-level program (fewest instructions), expressed
using the *full* machine (functions, pointers, recursion) — **bit-level
program/algorithm compression**.

**Mechanism — recursive black-boxes over dataflow relations**: a **black-box** = a
dataflow subgraph (a relation); **recursive** = compose relations bottom-up
(congruence); **reduce** = find equal relations (up to argument renaming) and share
one. This is **grammar compression (RePair/SEQUITUR) over bit-relations** =
**circuit minimization (ABC/Espresso) at the software level**.

**The search must be smart-and-cheap, not brute-force**: canonicalize each
sub-relation to a canonical boolean form (a BDD, or a truth-table over its input
bits) and **hash-cons** to find repeats — a hash lookup, not subgraph isomorphism
nor program enumeration; factor the most-repeated relation **greedily**
(RePair-style), scoring each factor by *savings minus argument-passing cost*. This
is grammar compression / CSE / e-graph extraction: polynomial graph exploration +
local synthesis.

**The ABI is itself a reduction mechanism**: a factored shared relation becomes a
**function call** — argument registers + a return. Procedural abstraction =
*factoring with arguments* = the v2 outliner's job (parameterized clones). So the
layers compose: **bit-level dataflow factoring** (leaf) → **function/ABI factoring**
(structural) → a synthesized program that *uses the machine's full expressiveness*.

**Two modes**: *program compression* (factor a given program) and *algorithm
compression* (shortest program for an I/O relation — superoptimization).

**Honest limits**: structural factoring ≠ entropy coding (complementary axes); the
optimum is NP-hard (SLP/smallest-grammar, Charikar et al. 2005) so it is approximated
greedily; the measure is relative to the minimal ISA.

**Bit operations should be loops, not unrolled chains.** A W-bit operation built
from gates is either W copies of its bit-slice (unrolled: a 64-bit add ≈ 64 × 35 ≈
2200 instructions) or *one* bit-slice plus loop overhead (≈ 35 + 20 ≈ 55) — a loop is
≈ W× smaller. Without an ALU, the loop index is a **walking-one shift register**
(pure copies), a **pointer traversal** (`peek`/`poke`), or a reusable **decrementer** —
a "loop ABI" symmetric with the function ABI. A bit-serial loop is the *ultimate
shared factor*: it factors one bit-slice across all W positions, parameterized by the
**bit index** — i.e. **loop synthesis = the inverse of unrolling**, and the
LZ-style decompressor-generator direction.

**Measured wins in-repo**: shared `add32` ~4×, rotation sharing −23% (36,865 →
28,272), exact-duplicate outlining −60% (233 → 94). The open target is *parameterized*
relations (same shape, renamed operands) — currently 0% on xorshift/md5.

### 8. The unified picture + open questions

One chain:

```
Hutter-prize compression ("compression = intelligence")
   → a bit-level minimal machine (RESM/BBJJ)
   → + peek/poke (indirect addressing) → a generic C-compiler target (software ALU + ABI)
   → the stable cell + data-reactivity + verifiability + data-relations
   → a reduction algorithm that synthesizes programs using the machine's full expressiveness
```

**Open questions**: parameterized sub-relation detection (v2 outliner); a symbolic
(BDD) canonical form that stays small for large inputs; loop summarization (fixed
points); confluence of the rewrite set; the derivative-ISA trade-off (Kolmogorov
invariance constant); the **factoring cost model for ported factors**
(argument-passing overhead, per the outliner's measured cost); the **approximation
ratio for bit-DAGs** (RePair's O(log n) bound is for strings, not ported graphs).

### 9. The seed, the colony, and the stream

Three deepening moves, one generator:

**The seed — compress the program itself.** Program and data are the same thing here
(bits in memory), so the program can be compressed like any data, and "run a program"
is just "copy bits into memory, then jump". Hence *self-modifying, interleaved
decompression* is native: a bootstrap decompresses a program, which decompresses more,
ultimately producing the end-user output. K(x) becomes a nested chain:
`|bootstrap| + Σ |compressed layerᵢ|`. Taken literally, "compression = intelligence"
becomes: **the ultimate compressor is a seed** — a small program that, executed, grows
into both the full program and the full data.

**The colony — several machines orchestrated.** The machine generalizes from "the
computer" to a *cell* in an orchestrated whole: a bounded graph of BBJJ instances
wired by an orchestrator (itself program), each talking through explicit I/O ports. K
generalizes to `|orchestrator| + Σ |program(Mᵢ)| + Σ |dataᵢ|`, and the seed becomes a
program that *spawns and wires a colony*. This is the composable-machines architecture
(§23–25): the system stays **knowable** only if the orchestration obeys
**acyclic/well-founded wiring + no shared mutable state across boundaries** (ports, not
aliasing). Two machines that alias the same cells break the rule and lose modular
knowability.

**The stream — the machine is an FST.** Bounded memory (8 Kbit) *forces* streaming:
the machine is a **finite-state transducer over bit streams** (`in` reads a bit, `out`
emits one; state = memory + instruction pointer). For streams, "halting" is replaced by
**equivalence** and **productivity**, both **decidable** (product construction;
silent-cycle check) — a *stronger* verifiability story. The Kolmogorov object for an
infinite stream is a **generator** (monotone/prefix complexity): the shortest finite
program that, run forever, emits the stream — exactly the decompressor-generator.
Streaming unifies the other moves: bit-serial loops (stream within a word), interleaved
layers (stream across layers), several machines (stream across machines).

**The honest limit.** A *single* bounded-memory FST computes only **regular
transductions**, but real decompression is not regular (LZ needs an unbounded
dictionary; context mixing an unbounded model). So a real decompressor's unbounded state
must be **distributed** — into the stream itself, a software stack, or several machines —
so that every *component* stays a bounded, decidable FST and only their *composition* is
unbounded. The seed, the colony, and the stream are three names for the same move:
**keep every part finite and verifiable; put the unboundedness in the composition.**

### 10. The Kolmogorov benchmark — competing against the decompression families

Full C-expressivity (`cc.py` + software ALU + software stack) means **every known
decompressor can be implemented on the machine** — so we can compare them not by ratio
but by the true Kolmogorov objective `K(x) = |decompressor| + |data|`.

The Hutter Prize (and x86 benchmarks) measure `|decompressor|` in *compiled C on a rich
ISA*, where it is dominated by compiler/ISA noise — so effectively only `|data|` (the
ratio) is measured, and the "program" half of K is invisible. On the minimal machine,
`|decompressor|` is counted in **bit-instructions** — the actual size of the algorithm —
so **both terms of K become faithful and comparable**.

The experiment: implement each family (LZ77/LZW dictionary, Huffman prefix, arithmetic
coding, BWT+MTF, PPM prediction, context mixing), measure `|program|` + `|data|` per
family on a corpus, and plot the **Pareto frontier** (program size × data size). The
striking, measurable question is the **Kolmogorov crossover point**: *at what corpus
size does a sophisticated algorithm's better ratio pay back its own program-size cost?*
On a minimal machine the likely answer is: **simple, loop-synthesized LZ wins for
surprisingly large inputs**, because its decompressor is a few hundred bit-instructions
while context mixing's is ~10⁵–10⁶. Loop synthesis (bit-serial, §29.7) is decisive: it
keeps the tiny decompressors tiny. The honest cost is the compiler effort to bit-serially
implement arithmetic coding / context mixing — the `cc.py` + loop-synthesis +
data-structures work.

### 11. Novelty & positioning (honest)

Most of the above is **well-known CS, correctly rediscovered and connected**; the
novelty is narrow and mostly still ahead (in results, not yet claimed).

**Not novel** (with sources): compression-as-intelligence (Hutter); minimal
single-instruction machines (BitBitJump / OISC); software ALU from gates/full-adders;
software call stack + ABI (cdecl); the stable cell (OCaml `ref`, Rust `Cell`, Clojure
`Atom`); data-reactivity (Solid/MobX/Vue); bounded → decidable halting (LBA);
dataflow / SSA / BDD; grammar compression (RePair/SEQUITUR); equality saturation
(`egg`); logic synthesis (ABC/Espresso); procedural abstraction (ICF/outlining);
superoptimization (Massalin/Souper/STOKE); bit-serial / loop synthesis; self-extraction,
layered bootstrapping, cellular automata, finite-state transducers.

**Partially novel** — (1) *RESM/BBJJ* as a specific OISC: copy + 2-way branch on a
path-selector bit, `@` peek/poke as an addressing mode, single-instruction yet a
generic C target; (2) the *Kolmogorov-benchmark lens* (§29.10): making `|program|` a
faithful measurable term via the minimal ISA — the freshest idea here; (3) the
*synthesis / framing* (origin → machine → ABI → cell / verifiability / data-relations →
reduction → seed / colony / stream), and the connection "outliner v2 = parameterized
sub-relation detection".

**Where real novelty would come from (still open):** a measured crossover point; an
approximation ratio for *ported* bit-DAGs (RePair's O(log n) is for strings); a
loop-synthesis technique for non-obvious bit-serial loops; a symbolic canonical form
that stays small for large inputs.

Net: ~90% known / connected, ~10% partially novel, and the citable result — if any —
is still in the experiments not yet run.

### 12. Performance: time = instructions (size ≈ speed)

The machine executes one instruction per step — no parallelism, no pipelining, no
cache — so the only speed metric is **instructions executed**. For straight-line code
every instruction runs once, so **time = program size**: the §29.7 size-reduction
(canonicalize + hash-cons + greedy factoring) *is* a performance optimizer. The only
genuine time/space axis is **loop vs. unroll** (loop = small+slow, unroll = big+fast) —
the Pebble-game / time-space trade-off (Pippenger 1981), roadmap direction 5. The
"atypical" discovery: on this machine, optimizing size and speed *collapse* (for
straight-line code), and the entire time/space trade-off reduces to loop/unroll
scheduling.

### 13. Evolution: mutation + verification (safe synthesis), and crossover = invention

Compile a proven codec, then mutate/rewrite its bit-level code and keep only
*verified* mutants — **randomized superoptimization** (STOKE) on the minimal machine,
made **safe** by §29.5 decidable equivalence (fixed-input: run it; all-input: FST
product construction). Seeding a **genetic algorithm** with proven codecs gives
**informed search**; 1-bit granularity gives the finest mutations. **Invention is
possible**: GP has genuinely invented (Koza's reinvented patents; NASA's ST5 antenna).
The distinction is *operator*, not *possibility*: **mutation = tuning** (local);
**crossover = recombination** (novel hybrids — genuine invention); the one rare case is
inventing a *brand-new primitive/abstraction* from scratch. On this machine the
**function ABI makes programs modular**, so **module-level crossover** (recombine
match-finder / dictionary / entropy-coder / predictor sub-routines) can hybridize
proven codecs — which is how cmix and LZMA were themselves composed. Fitness is exact
(size, speed = instructions, ratio); acceptance is a proof.

**Caveat — search is a dead end for exact-output synthesis.** The above applies to
*spec-based* synthesis (evolve toward a gradable spec, where partial correctness gives
a gradient). The decompressor-generator goal is *exact-output reproduction*: fitness is
all-or-nothing — a program 99% correct is still wrong, with no gradient toward the
last bit — so GA is no better than brute force. Evolving *program + data* together
collapses to a degenerate "store the data" solution. For exact output, the workable
path is **constructive, not search**: compile the algorithm (`cc.py`), then apply
*verified* rewrites / factoring / loop synthesis (monotonic, provably
behavior-preserving) — §29.7. Rule of thumb: *GA for "evolve toward a spec" — maybe;
GA for "reproduce this exact output" — no.*


### 14. The tower of abstraction: ideas span layers

The machine is the *bottom* of a tower, and inventions live *up* the tower:

```
L5 ideas            compression-as-modeling · Kolmogorov · seed/colony/stream
L4 codec concepts   dictionary · entropy coder · predictor · transform
L3 algorithms       arrays · pointers · lists · hashing · recursion
L2 C-like language  types · operators · statements          (cc.py)
L1 ABI              functions · call stack · return         (callstack_gen.py)
L0 bits             copy + branch (+ peek/poke addressing)   (RESM/BBJJ)
```

Each layer compiles down to the one below, and each has its *own* building blocks,
operators, and kind of invention (L0 micro-mutations … L4 module recombination … L5
conceptual leaps). So the search/evolution must **span layers with layer-matched
operators**: a dictionary is an L4 concept — unreachable by L0 bit-mutations, but
reachable by L4 module crossover. "Intelligence" is a high-layer property (L4/L5),
*realized* — not created — by the low-level code. The machine's value is being the
clearest possible **floor** (one instruction, one bit) for the whole tower.

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
