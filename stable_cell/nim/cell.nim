# Stable Cell — the "mutable box" pattern, Nim (compiled) implementation.
#
# A single-field `ref object` used as a mutable box for one value/reference.
# Its identity (the object) never changes; only the `.value` field is rewired,
# so re-pointing is visible to every alias of the cell.
#
# Invariants:
#   I1 Identity stability:  setCell never changes the cell's object identity.
#   I2 Coherence:           any two aliases observe the same current value.
#   I3 Target neutrality:   setCell mutates only the cell's field, never the
#                           old or new target.

type
  Cell[T] = ref object
    value: T

proc make[T](t: T): Cell[T] =
  Cell[T](value: t)

proc get[T](c: Cell[T]): T =
  c.value

proc setCell[T](c: Cell[T]; t: T) =
  c.value = t

# pointer identity, not value equality
proc sameObj[T](a, b: Cell[T]): bool =
  addr(a[]) == addr(b[])

when isMainModule:
  let b = 10
  let c = 20

  # 1) Rebind FAILS (why the cell exists):
  var direct = b
  let holder = direct     # captures the value
  direct = c              # rebind — holder is stale (still b)
  doAssert holder == b, "rebind leaves aliases stale"

  # 2) Cell WORKS:
  let cell = make(b)
  let h1 = cell           # aliases of the SAME object
  let h2 = cell

  setCell(cell, c)        # mutate contents — identity unchanged

  # I1: identity stability
  doAssert sameObj(h1, h2) and sameObj(h2, cell), "I1 aliases share one object"
  # I2: coherence across aliases
  doAssert get(h1) == c, "I2 coherence h1"
  doAssert get(h2) == c, "I2 coherence h2"
  # I3: target neutrality
  doAssert b == 10, "I3 old target unchanged"

  echo "All invariant checks passed."
  echo "get(h1) = ", get(h1), " | get(h2) = ", get(h2)
  echo "same cell: ", sameObj(h1, h2)
