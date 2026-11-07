#!/usr/bin/env python3
"""
RESM / BBJJ ("Bit Bit Jump Jump") — the real copyjumpvm machine, faithfully
re-implemented from the repo's CopyJumpMachine.py semantics.

Textual instruction set:
  m <dst> <src>           copy one bit from <src> to <dst>
  j <label>               unconditional jump
  j <cond> <label>        if bit(<cond>) == 0 -> next instruction, else -> label
  j <cond> <l0> <l1>      2-way branch: bit==0 -> l0, bit==1 -> l1
  l <label>               label (not an instruction)

Memory: 8192 bits (13-bit addresses); bits 0 and 1 are CONSTANT sources
([0, 1] prepended), i.e. reading bit 0 always yields 0 and bit 1 always yields 1.
'@' prefix = dereference: read ADDR_BITS bits LSB-first as the actual address
(indirect read/write = peek/poke, and computed goto).
'in' / 'out' = single-bit I/O.

Termination: the instruction pointer falls off the end of the program.
Infinite-loop detection: a repeated (ip, memory) snapshot => infinite loop
(this is the repo's "iloop vs tloop" behaviour).
"""

ADDR_BITS = 13
MEM_BITS = 1024 * 8  # 8192 bits


class Machine:
    def __init__(self, code):
        # bits 0 and 1 are constant 0/1 sources
        self.memory = [0, 1] + [0] * (MEM_BITS - 2)
        self.lines, self.labels = self._parse(code)

    def _parse(self, code):
        lines = []
        labels = {}
        for line in code.split("\n"):
            line = line.split("#")[0].strip()
            if not line:
                continue
            ops = line.split(" ")
            if ops[0] == "l":
                labels[ops[1]] = len(lines)
            else:
                lines.append(ops)
        labels["end"] = len(lines)
        return lines, labels

    def addr_of(self, base):
        """Read ADDR_BITS bits (LSB-first) starting at `base` as an address."""
        a = 0
        for i in range(ADDR_BITS):
            a |= self.memory[base + i] << i
        return a

    def read(self, src):
        if src.startswith("@"):
            src = str(self.addr_of(int(src[1:])))
        if src == "in":
            return int(input("bit? "))
        return self.memory[int(src)]

    def write(self, dst, bit):
        if dst.startswith("@"):
            dst = str(self.addr_of(int(dst[1:])))
        if dst == "out":
            print("bit:", bit)
            return
        self.memory[int(dst)] = bit

    def _jump(self, cline, ip):
        if len(cline) == 2:                     # unconditional
            if cline[1].startswith("@"):        # computed goto
                new_ip = self.addr_of(int(cline[1][1:]))
            else:
                new_ip = cline[1]
        elif len(cline) == 3:                   # bit==0 -> next, bit==1 -> label
            new_ip = "next" if self.read(cline[1]) == 0 else cline[2]
        elif len(cline) == 4:                   # bit==0 -> l0, bit==1 -> l1
            new_ip = cline[2] if self.read(cline[1]) == 0 else cline[3]
        else:
            raise ValueError(f"bad jump: {cline}")
        if new_ip == "next":
            return ip + 1
        try:
            return int(new_ip)
        except ValueError:
            return self.labels[new_ip]

    def run(self, detect_loop=True):
        ip = 0
        seen = set()
        steps = 0
        while ip < len(self.lines):
            if detect_loop:
                snap = (ip, bytes(self.memory))
                if snap in seen:
                    return "infinite loop", steps
                seen.add(snap)
            cline = self.lines[ip]
            if cline[0] == "m":
                self.write(cline[1], self.read(cline[2]))
                ip += 1
            elif cline[0] == "j":
                ip = self._jump(cline, ip)
            else:
                raise ValueError(f"bad instruction: {cline}")
            steps += 1
        return "terminated", steps


def demo():
    def show(name, code):
        m = Machine(code)
        status, steps = m.run()
        print(f"{name}: status={status} ({steps} steps); "
              f"mem[2]={m.memory[2]}, mem[3]={m.memory[3]}")

    show("copy_constant (terminates)", "m 2 1")
    show("iloop (infinite loop detected)", "l loop\nj loop")
    show("tloop (conditional loop, terminates)",
         "m 2 1\nj 2 done loop\nl loop\nm 2 0\nj 2 done loop\nl done\nm 3 1")


if __name__ == "__main__":
    demo()
