#!/usr/bin/env python3
"""
bbjj_reduce.py — a VERIFIED peephole / size reducer for RESM/BBJJ programs.

SYNERGY between the two repos: this lives in python_mutable_chain_technique (the
theory repo) but runs on REAL copyjumpvm `.prg.txt` programs (the research repo),
applying semantics-preserving rewrites and VERIFYING each by checking bit-identical
output on a set of test inputs (all 2^k input sequences over the program's `in`
bits).

Rewrites:
  R1 self-copy elimination : `m x x` (numeric dst==src) is a no-op.
  R2 jump-to-next          : `j L` immediately followed by `l L` is a no-op.
  R3 try-remove            : remove any single instruction, keep iff behavior is
                             preserved on ALL test inputs (test-vector check, not a
                             full proof — same trade-off as Massalin 1987).
"""

import sys
from itertools import product

ADDR_BITS = 13
MEM_BITS = 1 << ADDR_BITS


def is_num(s):
    return s.lstrip("-").isdigit()


def parse_items(code):
    items = []
    for raw in code.split("\n"):
        line = raw.split("#")[0].strip()
        if not line:
            continue
        p = line.split()
        if p[0] == "l":
            items.append(("label", p[1]))
        elif p[0] == "m":
            items.append(("m", p[1], p[2]))
        elif p[0] == "j":
            items.append(("j",) + tuple(p[1:]))
        else:
            raise ValueError("unrecognized line: " + line)
    return items


def items_to_run(items):
    lines, labels = [], {}
    for it in items:
        if it[0] == "label":
            labels[it[1]] = len(lines)
        else:
            lines.append(it)
    labels["end"] = len(lines)
    return lines, labels


def run(lines, labels, input_bits):
    """Return (output_bits, status, steps, in_consumed)."""
    mem = [0, 1] + [0] * (MEM_BITS - 2)
    ip = 0
    out = []
    in_idx = 0
    seen = set()
    steps = 0

    def addr_of(base):
        a = 0
        for i in range(ADDR_BITS):
            a |= mem[base + i] << i
        return a

    def read(src):
        nonlocal in_idx
        if src.startswith("@"):
            src = str(addr_of(int(src[1:])))
        if src == "in":
            b = input_bits[in_idx] if in_idx < len(input_bits) else 0
            in_idx += 1
            return b
        return mem[int(src)]

    def write(dst, bit):
        if dst.startswith("@"):
            dst = str(addr_of(int(dst[1:])))
        if dst == "out":
            out.append(bit)
        else:
            mem[int(dst)] = bit

    while ip < len(lines):
        snap = (ip, bytes(mem))
        if snap in seen:
            return tuple(out), "loop", steps, in_idx
        seen.add(snap)
        cline = lines[ip]
        if cline[0] == "m":
            write(cline[1], read(cline[2]))
            ip += 1
        elif cline[0] == "j":
            args = cline[1:]
            if len(args) == 1:
                new_ip = addr_of(int(args[0][1:])) if args[0].startswith("@") else args[0]
            elif len(args) == 2:
                new_ip = "next" if read(args[0]) == 0 else args[1]
            elif len(args) == 3:
                new_ip = args[1] if read(args[0]) == 0 else args[2]
            else:
                raise ValueError(str(cline))
            if new_ip == "next":
                ip += 1
            else:
                try:
                    ip = int(new_ip)
                except ValueError:
                    ip = labels[new_ip]
        else:
            raise ValueError(str(cline))
        steps += 1
    return tuple(out), "terminated", steps, in_idx


def count_instr(items):
    return sum(1 for it in items if it[0] != "label")


def detect_k(items):
    lines, labels = items_to_run(items)
    _, _, _, consumed = run(lines, labels, [0] * 64)
    return consumed


def behavior(items, inputs):
    lines, labels = items_to_run(items)
    return [run(lines, labels, list(inp))[:2] for inp in inputs]  # (out, status)


def render(items):
    out = []
    for it in items:
        if it[0] == "label":
            out.append("l " + it[1])
        elif it[0] == "m":
            out.append("m %s %s" % (it[1], it[2]))
        else:
            out.append("j " + " ".join(it[1:]))
    return "\n".join(out) + "\n"


def reduce(items, inputs, max_passes=200):
    base = behavior(items, inputs)
    removed = 0
    passes = 0
    while passes < max_passes:
        passes += 1
        changed = False
        out = []
        i = 0
        while i < len(items):
            it = items[i]
            if it[0] == "m" and is_num(it[1]) and it[1] == it[2]:
                changed = True; removed += 1; i += 1; continue            # R1
            if (it[0] == "j" and len(it) == 2 and i + 1 < len(items)
                    and items[i + 1] == ("label", it[1])):
                changed = True; removed += 1; i += 1; continue            # R2
            out.append(it); i += 1
        items = out
        i = 0
        while i < len(items):                                            # R3
            if items[i][0] == "label":
                i += 1; continue
            cand = items[:i] + items[i + 1:]
            if behavior(cand, inputs) == base:
                changed = True; removed += 1
                items = cand
            else:
                i += 1
        if not changed:
            break
    return items, passes, removed


def main():
    if len(sys.argv) < 2:
        print("usage: python3 bbjj_reduce.py <program.prg.txt> [out.prg.txt]")
        return
    code = open(sys.argv[1]).read()
    items = parse_items(code)
    k = detect_k(items)
    inputs = [list(seq) for seq in product([0, 1], repeat=k)]
    before = count_instr(items)
    reduced, passes, removed = reduce(items, inputs)
    after = count_instr(reduced)
    ok = behavior(reduced, inputs) == behavior(items, inputs)
    print("file: %s" % sys.argv[1])
    print("input bits: %d; test inputs: %d" % (k, len(inputs)))
    print("instructions: %d -> %d (removed %d, %d pass(es))" % (before, after, removed, passes))
    print("verified bit-identical on all inputs: %s" % ok)
    out_path = sys.argv[2] if len(sys.argv) > 2 else sys.argv[1].replace(".prg.txt", ".reduced.prg.txt")
    open(out_path, "w").write(render(reduced))
    print("wrote: %s" % out_path)


if __name__ == "__main__":
    main()
