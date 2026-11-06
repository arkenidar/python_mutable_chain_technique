# Deterministic reclamation under --mm:arc / --mm:orc.
#
# Nim's ARC/ORC reference-counts `ref` types at compile time; the `=destroy`
# hook fires the moment a ref's count drops to zero — the "free" event.
# This is the compiled counterpart to the manual acquire/release in
# cell/js/refcounted_cell.js.
#
# NOTE: destructors are NOT deterministic under refc/markAndSweep/boehm —
#       this demo must be compiled with --mm:arc or --mm:orc.

type
  PayloadObj = object
    name: string
  Payload = ref PayloadObj
  Cell = ref object
    value: Payload

# `=destroy` must be declared on the OBJECT type (not the ref); ARC/ORC
# calls it when the ref's count drops to zero.
proc `=destroy`(p: var PayloadObj) =
  echo "reclaimed: ", p.name

when isMainModule:
  # 1) scope-exit reclamation (RAII):
  block:
    let b = Payload(name: "B")   # only owner; destroyed at block end
  echo "b's block ended"         # "reclaimed: B" prints before this line

  # 2) re-point -> reclaim (stable cell + ARC):
  block:
    let cell = Cell(value: Payload(name: "C"))
    cell.value = Payload(name: "D")  # releases old "C" => "reclaimed: C"
    echo "cell now holds: ", cell.value.name
    # "reclaimed: D" fires when `cell` leaves scope at block end
