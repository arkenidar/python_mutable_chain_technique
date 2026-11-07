"""
Fixed-input superoptimizer for the copy-jump-peek/poke machine.

Finds the shortest program behaviorally equivalent to a *target* program on a
*fixed* initial memory (one input). Equivalence is EXACT: two programs are
equivalent on that input iff they produce the same final memory and the same
termination status (halt/loop), checked by reachable-state cycle detection.

Caveat (accuracy): this is FIXED-input equivalence, not all-input. It is the
correct and sufficient notion for inputless programs (e.g. a decompressor judged
on one payload - see docs/CONVERSATION.md §27), but it is NOT a proof of
equivalence for arbitrary inputs.

The search is enumerative over all instruction forms (~801) in increasing length
(801^n): length 1 ~ 801, length 2 ~ 642k, length 3 ~ 514M. So it is "exact but
combinatorial" - the verification is cheap; the search is the wall.
"""

from itertools import product

from vm import N, V, successors


def all_instructions():
    """All instruction forms (3-tuples (op, a, b)) over N cells / V values."""
    ins = []
    for a in range(N):
        for v in range(V):
            ins.append(("poke", a, v))
    for d in range(N):
        for s in range(N):
            ins.append(("copy", d, s))
    for a in range(N):
        ins.append(("peek", a, 0))
    for a in range(N):
        for t in range(N):
            ins.append(("jz", a, t))
    for t in range(N):
        ins.append(("jump", t, 0))
    ins.append(("halt", 0, 0))
    return ins


def run_result(program, memory):
    """(final_memory_tuple, status) with status in {'halt', 'loop'}."""
    state = (0, tuple(memory))
    seen = set()
    while True:
        if state in seen:
            return state[1], "loop"
        seen.add(state)
        nxt = successors(state, program)
        if not nxt:
            return state[1], "halt"
        state = nxt[0]


def superoptimize(target, memory, max_len=2):
    """Return (shortest_equivalent_program, length, examined) or (None, None, None)."""
    ins = all_instructions()
    target_result = run_result(target, memory)
    for length in range(1, max_len + 1):
        examined = 0
        for combo in product(ins, repeat=length):
            examined += 1
            if run_result(list(combo), memory) == target_result:
                return list(combo), length, examined
    return None, None, None


def demo():
    forms = all_instructions()
    print("=== fixed-input superoptimizer (copy-jump-peek/poke machine) ===")
    print(f"machine: {N} cells x {V} values; instruction forms: {len(forms)}")
    print(f"search space: length 1={len(forms)}, 2={len(forms)**2}, 3={len(forms)**3}")
    print()

    # Demo 1: dead-store elimination (redundant poke overwritten before any use)
    target = [("poke", 0, 5), ("poke", 0, 7), ("halt", 0, 0)]
    mem = [0] * N
    found, length, examined = superoptimize(target, mem, max_len=1)
    print("Demo 1 - dead-store elimination:")
    print(f"  target   : {target}")
    print(f"  behavior : {run_result(target, mem)}")
    print(f"  shortest : {found}  (length {length}, examined {examined})")
    print(f"  behavior : {run_result(found, mem) if found else None}")
    print()

    # Demo 2: fixed-input-only reduction (NOT valid for all inputs)
    target2 = [("copy", 0, 1), ("halt", 0, 0)]   # cell0 <- cell1
    mem2 = [0] * N                                # cell1 happens to be 0
    found2, length2, examined2 = superoptimize(target2, mem2, max_len=1)
    print("Demo 2 - fixed-input-only reduction (NOT all-input valid):")
    print(f"  target   : {target2}")
    print(f"  behavior : {run_result(target2, mem2)}")
    print(f"  shortest : {found2}  (length {length2}, examined {examined2})")
    print(f"  behavior : {run_result(found2, mem2) if found2 else None}")
    print("  NOTE: valid only because cell1 == 0 in the fixed input; with cell1=5")
    print("        the target copies 5 to cell0 but the shortest does not, so this")
    print("        is FIXED-input, not all-input, equivalence.")


if __name__ == "__main__":
    demo()
