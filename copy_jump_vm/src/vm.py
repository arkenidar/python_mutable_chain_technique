"""
Copy-Jump-Peek/Poke machine over a bounded address space.

Turns the theory in ../../docs/CONVERSATION.md (sections 17-22) into something
runnable: finite control + bounded memory => liveness is decidable, so
reclamation can be proven exactly by exhaustive reachability (no GC, no
refcount).

Instructions (memory of N cells, values 0..V-1):
  poke addr val   memory[addr] = val             (allocation)
  copy dst src    memory[dst]  = memory[src]     (move)
  peek addr       read memory[addr]              (use)
  jz  addr tgt    if memory[addr] == 0: pc = tgt (branch)
  jump tgt        pc = tgt
  halt            stop
"""

from collections import deque

N = 16   # address bits = 4 (a nibble)
V = 16   # value bits  = 4


def successors(state, program):
    """All successor states of `state` (at most one)."""
    pc, mem = state
    if pc >= len(program):
        return []
    op = program[pc][0]
    m = list(mem)
    if op == "poke":
        _, addr, val = program[pc]
        m[addr] = val
        return [(pc + 1, tuple(m))]
    if op == "copy":
        _, dst, src = program[pc]
        m[dst] = m[src]
        return [(pc + 1, tuple(m))]
    if op == "peek":
        return [(pc + 1, tuple(m))]
    if op == "jz":
        _, addr, tgt = program[pc]
        return [(tgt if m[addr] == 0 else pc + 1, tuple(m))]
    if op == "jump":
        _, tgt = program[pc]
        return [(tgt, tuple(m))]
    if op == "halt":
        return []
    raise ValueError(f"unknown op: {program[pc]!r}")


def reads(ins):
    """Addresses read (used) by an instruction."""
    if ins[0] == "peek":
        return [ins[1]]
    if ins[0] == "copy":
        return [ins[2]]          # src
    if ins[0] == "jz":
        return [ins[1]]
    return []


def explore(program, memory):
    """Exhaustively explore reachable states (finite: memory is bounded).
    Returns (status, graph) with status in {'halts', 'loops'}."""
    init = (0, tuple(memory))
    graph = {init: set()}
    queue = deque([init])
    halts = False
    while queue:
        s = queue.popleft()
        pc, _ = s
        if pc < len(program) and program[pc][0] == "halt":
            halts = True
        for t in successors(s, program):
            if t not in graph:
                graph[t] = set()
                queue.append(t)
            graph[s].add(t)
    return ("halts" if halts else "loops"), graph


def live_cells(program, graph):
    """Backward liveness: for each state, cells read on some future path."""
    live = {s: set() for s in graph}
    changed = True
    while changed:
        changed = False
        for s in graph:
            new = set()
            pc, _ = s
            if pc < len(program):
                new.update(reads(program[pc]))
            for t in graph[s]:
                new.update(live[t])
            if not new <= live[s]:
                live[s] |= new
                changed = True
    return live


def reclamation_demo():
    program = [
        ("poke", 0, 5),   # allocate cell 0
        ("peek", 0),      # use cell 0
        ("poke", 1, 7),   # allocate cell 1
        ("peek", 1),      # use cell 1
        ("peek", 0),      # use cell 0 again (live until here)
        ("halt",),
    ]
    memory = [0] * N
    status, graph = explore(program, memory)
    live = live_cells(program, graph)

    print("reclamation demo (poke = allocate, peek = use)")
    print(f"  memory: {N} cells x {V} values; termination: {status}; "
          f"reachable states: {len(graph)}")
    print("  live = cells read now or on some future path:")
    s = (0, tuple(memory))
    while True:
        pc, _ = s
        if pc >= len(program):
            break
        print(f"    {pc}: {str(program[pc]):20} live={sorted(live[s])}")
        nxt = successors(s, program)
        if not nxt:
            break
        s = nxt[0]
    print("  => cell 1 is dead (reclaimable) after pc=3 (its last peek)")
    print("  => cell 0 is dead (reclaimable) after pc=4 (its last peek)")


def loop_demo():
    program = [("jump", 0)]   # pc=0 -> pc=0 forever
    status, graph = explore(program, [0] * N)
    print("loop demo: [('jump', 0)]")
    print(f"  termination: {status} (cycle detected over {len(graph)} state(s))")


if __name__ == "__main__":
    reclamation_demo()
    print()
    loop_demo()
