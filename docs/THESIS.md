# The unified research program — from Hutter-prize compression to a bit-level minimal machine that unifies the stable cell, verifiability, and data-relations, and yields a reduction algorithm

> This is the **canonical synthesis** — the distilled, stable statement of the idea. The full raw discourse (every idea, correction, and open question) is archived in [`CONVERSATION.md`](CONVERSATION.md); see [`INDEX.md`](INDEX.md) for the reading guide.

## 1. Origin — the Hutter Prize (compression as intelligence)

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

## 2. The machine — maximum flexibility at the minimum: RESM/BBJJ

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

## 3. The added peek/poke — indirect addressing → pointers → C-style functions (an ABI)

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

## 4. Toward data-reactivity and the stable-cell primitive

A peek/poke-addressable bit **is a stable cell**: its *identity* (the address) is
fixed while its *contents* (the bit) can be rewritten — the `[t]` / `c[0]` /
`c[0] = t` primitive (docs §2–3) at bit granularity. `peek` = get, `poke` = set.

- **Data-reactivity** (docs §6): the machine's state is a set of reactive cells; a
  "signal" is a cell plus change notification. On the minimal machine, the
  *rewiring axis* (the stable cell) and *memory access* are the same thing.
- **Two orthogonal axes** (docs §4): *lifetime/ownership* (when to reclaim a bit)
  vs *mutability/rewiring* (the cell). Both are visible at the bit level; the
  stable cell is the rewiring axis, and indirect addressing makes cells first-class.

## 5. Resurrecting program verifiability

Because memory is bounded, the machine has **finitely many configurations**, so
**liveness/halting is decidable** by cycle detection (docs §19–22) — the VM's
`iloop → "infinite loop"`, `tloop → "terminated"` is the concrete form.
Boundedness is thus a *design principle that buys knowability* (§24), and
compositional knowability needs exactly acyclic/well-founded structure + no shared
mutable state (§25). A bounded machine + fixed input makes **verification** cheap
("run it once"), leaving **search** as the hard part (§26–27).

## 6. Resurrecting data-relations

A program is **not an opaque I/O function** — it is a **dataflow graph of bit-level
relations**: a copy `m dst src` is an *identity* relation (`dst := src`); a gate is
a *boolean* relation (`out := f(in1, in2)`); a constant is a relation to bit 0 or 1.

So the **summary of a fragment** is its *derivation relation* — "these output bits
are defined by these expressions over the input bits" — the *intensional* structure,
not the extensional I/O table. This is what makes reduction factorable: sharing a
repeated *relation* (a gate, an adder) is the whole game.

## 7. Algorithm-making — a proper algorithm for reduction

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

## 8. The unified picture + open questions

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

## 9. The seed, the colony, and the stream

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

## 10. The Kolmogorov benchmark — competing against the decompression families

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

## 11. Novelty & positioning (honest)

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

## 12. Performance: time = instructions (size ≈ speed)

The machine executes one instruction per step — no parallelism, no pipelining, no
cache — so the only speed metric is **instructions executed**. For straight-line code
every instruction runs once, so **time = program size**: the §29.7 size-reduction
(canonicalize + hash-cons + greedy factoring) *is* a performance optimizer. The only
genuine time/space axis is **loop vs. unroll** (loop = small+slow, unroll = big+fast) —
the Pebble-game / time-space trade-off (Pippenger 1981), roadmap direction 5. The
"atypical" discovery: on this machine, optimizing size and speed *collapse* (for
straight-line code), and the entire time/space trade-off reduces to loop/unroll
scheduling.

## 13. Evolution: mutation + verification (safe synthesis), and crossover = invention

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


## 14. The tower of abstraction: ideas span layers

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


