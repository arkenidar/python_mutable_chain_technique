# Copy-Jump-Peek/Poke machine over a bounded address space (Nim port of vm.py).
#
# Turns the theory in ../../docs/CONVERSATION.md (sections 17-22) into something
# runnable: finite control + bounded memory => liveness is decidable, so
# reclamation can be proven exactly by exhaustive reachability (no GC, no refcount).
#
# Instructions (memory of N cells, values 0..V-1):
#   poke addr val   memory[addr] = val             (allocation)
#   copy dst src    memory[dst]  = memory[src]     (move)
#   peek addr       read memory[addr]              (use)
#   jz  addr tgt    if memory[addr] == 0: pc = tgt (branch)
#   jump tgt        pc = tgt
#   halt            stop

import std/[sets, deques, sequtils, algorithm]

const N = 16   # address bits = 4 (a nibble)
const V = 16   # value bits  = 4

type
  Inst = tuple[op: string, a: int, b: int]
  Program = seq[Inst]
  State = tuple[pc: int, mem: seq[int]]

proc show(ins: Inst): string =
  case ins.op
  of "poke": "poke " & $ins.a & " " & $ins.b
  of "copy": "copy " & $ins.a & " " & $ins.b
  of "peek": "peek " & $ins.a
  of "jz": "jz " & $ins.a & " " & $ins.b
  of "jump": "jump " & $ins.a
  of "halt": "halt"
  else: ins.op

proc successors(state: State, prog: Program): seq[State] =
  if state.pc >= prog.len:
    return @[]
  let ins = prog[state.pc]
  var m = state.mem
  case ins.op
  of "poke":
    m[ins.a] = ins.b
    result = @[(pc: state.pc + 1, mem: m)]
  of "copy":
    m[ins.a] = m[ins.b]
    result = @[(pc: state.pc + 1, mem: m)]
  of "peek":
    result = @[(pc: state.pc + 1, mem: m)]
  of "jz":
    let next = if m[ins.a] == 0: ins.b else: state.pc + 1
    result = @[(pc: next, mem: m)]
  of "jump":
    result = @[(pc: ins.a, mem: m)]
  of "halt":
    result = @[]
  else:
    raise newException(ValueError, "unknown op: " & ins.op)

proc reads(ins: Inst): seq[int] =
  case ins.op
  of "peek": @[ins.a]
  of "copy": @[ins.b]     # src
  of "jz": @[ins.a]
  else: @[]

proc indexOf(states: seq[State], s: State): int =
  for i, t in states:
    if t == s:
      return i
  return -1

proc explore(prog: Program, memory: seq[int]):
    tuple[status: string, states: seq[State], succs: seq[seq[int]]] =
  var states: seq[State] = @[(pc: 0, mem: memory)]
  var succs: seq[seq[int]] = @[]
  succs.add(@[])
  var queue = initDeque[int]()
  queue.addLast(0)
  var halts = false
  while queue.len > 0:
    let i = queue.popFirst()
    let s = states[i]
    if s.pc < prog.len and prog[s.pc].op == "halt":
      halts = true
    var ss: seq[int] = @[]
    for t in successors(s, prog):
      var j = indexOf(states, t)
      if j < 0:
        j = states.len
        states.add(t)
        succs.add(@[])
        queue.addLast(j)
      ss.add(j)
    succs[i] = ss
  result.status = (if halts: "halts" else: "loops")
  result.states = states
  result.succs = succs

proc liveCells(prog: Program, states: seq[State],
               succs: seq[seq[int]]): seq[HashSet[int]] =
  result = newSeq[HashSet[int]](states.len)
  for i in 0 ..< states.len:
    result[i] = initHashSet[int]()
  var changed = true
  while changed:
    changed = false
    for i in 0 ..< states.len:
      var newSet = initHashSet[int]()
      let s = states[i]
      if s.pc < prog.len:
        for a in reads(prog[s.pc]):
          newSet.incl(a)
      for j in succs[i]:
        for a in result[j]:
          newSet.incl(a)
      for a in newSet:
        if a notin result[i]:
          result[i].incl(a)
          changed = true

proc sortedLive(h: HashSet[int]): seq[int] =
  result = toSeq(h)
  result.sort()

proc reclamationDemo() =
  let prog: Program = @[
    ("poke", 0, 5),   # allocate cell 0
    ("peek", 0, 0),   # use cell 0
    ("poke", 1, 7),   # allocate cell 1
    ("peek", 1, 0),   # use cell 1
    ("peek", 0, 0),   # use cell 0 again (live until here)
    ("halt", 0, 0),
  ]
  let res = explore(prog, newSeq[int](N))
  let live = liveCells(prog, res.states, res.succs)
  echo "reclamation demo (poke = allocate, peek = use)"
  echo "  memory: ", N, " cells x ", V, " values; termination: ", res.status,
       "; reachable states: ", res.states.len
  echo "  live = cells read now or on some future path:"
  var i = 0
  while i < res.states.len:
    let s = res.states[i]
    if s.pc >= prog.len:
      break
    echo "    ", s.pc, ": ", show(prog[s.pc]), "  live=", sortedLive(live[i])
    if res.succs[i].len == 0:
      break
    i = res.succs[i][0]
  echo "  => cell 1 is dead (reclaimable) after pc=3 (its last peek)"
  echo "  => cell 0 is dead (reclaimable) after pc=4 (its last peek)"

proc loopDemo() =
  let prog: Program = @[("jump", 0, 0)]   # pc=0 -> pc=0 forever
  let res = explore(prog, newSeq[int](N))
  echo "loop demo: [('jump', 0)]"
  echo "  termination: ", res.status, " (cycle detected over ",
       res.states.len, " state(s))"

when isMainModule:
  reclamationDemo()
  echo ""
  loopDemo()
