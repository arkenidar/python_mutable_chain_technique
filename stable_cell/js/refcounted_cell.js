/**
 * Refcounted Cell — stable cell + manual reference counting (JavaScript demo).
 *
 * Demonstrates how the "[t] / c[0] / c[0] = t" cell composes with a reference
 * count to produce reclamation that is *triggered by the re-point itself*:
 *
 *     set(cell, newTarget)  ->  acquire(newTarget), release(oldTarget)
 *
 * This mirrors the C++ Ref<T> design (Ref holds a shared_ptr<T>):
 * the cell supplies the stable edge; the refcount supplies the reclamation.
 *
 * CAVEAT: naive reference counting cannot collect cycles. If targets point
 * back at each other, their counts never reach zero and they leak — which is
 * exactly why the doubly-linked "mutable chain" needs tracing GC, weak
 * back-links, or an arena. (Real JavaScript uses a tracing GC instead.)
 */

// make(t) -> cell holding t, with one reference acquired
function make(t) {
    acquire(t);
    return [t];
}

// get(c) -> current target
function get(c) {
    return c[0];
}

// set(c, t) -> re-point the cell AND update reference counts, so the old
// target is reclaimed the moment its count drops to zero.
function set(c, t) {
    const old = c[0];
    c[0] = t;      // stable-cell re-point (identity preserved)
    acquire(t);    // new target gains a reference
    release(old);  // old target loses one — maybe reclaimed now
}

function acquire(x) {
    x.__count = (x.__count || 0) + 1;
}

function release(x) {
    x.__count -= 1;
    if (x.__count === 0) {
        console.log("reclaimed:", x.name);
    }
}

// --- demonstration ---
function main() {
    console.log("1) re-point -> reclaim (cell + refcount):");
    const B = { name: "B" };
    const C = { name: "C" };
    const cell = make(B);   // B.__count = 1
    set(cell, C);           // re-point: release(B) => "reclaimed: B", acquire(C)
    console.log("   cell now holds:", get(cell).name, "(C.__count =", C.__count + ")");

    console.log("\n2) cycle caveat (naive refcount cannot collect cycles):");
    const A = { name: "A", other: null };
    const D = { name: "D", other: null };
    acquire(A);             // external root holds A
    acquire(D);             // external root holds D
    A.other = D; acquire(D); // A -> D
    D.other = A; acquire(A); // D -> A  (now a cycle)
    release(A);             // drop root: A 2 -> 1
    release(D);             // drop root: D 2 -> 1
    // Each is still held by the other's `.other`, so counts never reach zero:
    console.log("   A.__count =", A.__count, "| D.__count =", D.__count, "-> cycle leak");
    console.log("   (tracing GC / weak refs / arena needed for cycles)");
}

main();
