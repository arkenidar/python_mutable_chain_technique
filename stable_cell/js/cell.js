/**
 * Stable Cell — the "[t] / c[0] / c[0] = t" pattern (JavaScript).
 *
 * A one-element array used as a mutable box for a single reference.
 * Its identity (the array object) never changes; only slot [0] is rewired,
 * so re-pointing is visible to every alias of the cell.
 *
 * Invariants:
 *   I1 Identity stability:  setCell never changes the cell's object identity.
 *   I2 Coherence:           any two aliases observe the same current target.
 *   I3 Target neutrality:   setCell mutates only slot [0], never the targets.
 */

// make(t) -> cell holding t
function make(t) {
    return [t];
}

// get(c) -> current target
function get(c) {
    return c[0];
}

// setCell(c, t) -> redirect c to t (mutates slot [0], never rebinds c)
// (named setCell to avoid confusion with the global Set constructor)
function setCell(c, t) {
    c[0] = t;
}

// --- minimal assertion helper ---
function assertEq(actual, expected, msg) {
    if (actual !== expected) {
        throw new Error(`${msg}: expected ${expected}, got ${actual}`);
    }
}

// --- demonstration / proof ---
function main() {
    const B = Object.freeze({ name: "B" }); // frozen => proves we never mutate targets
    const C = { name: "C" };

    // 1) Rebind FAILS (why the cell exists):
    let direct = B;
    const holder = direct;    // alias captured the object
    direct = C;               // rebind — holder is stale
    assertEq(holder.name, "B", "rebind leaves aliases stale");

    // 2) Cell WORKS:
    const cell = make(B);     // [B]
    const h1 = cell;          // aliases of the SAME array object
    const h2 = cell;
    cell.tag = "stable";      // marker to prove the object identity persists

    setCell(cell, C);         // mutate slot [0] — no rebinding

    // I1: same array object (a fresh array would not carry `.tag`)
    assertEq(cell.tag, "stable", "I1 identity stability");
    assertEq(h1 === h2 && h2 === cell, true, "I1 aliases share one object");

    // I2: coherent across aliases
    assertEq(get(h1).name, "C", "I2 coherence h1");
    assertEq(get(h2).name, "C", "I2 coherence h2");

    // I3: targets untouched (B is frozen and was never mutated)
    assertEq(B.name, "B", "I3 old target unchanged");

    console.log("All invariant checks passed.");
    console.log("get(h1) =", get(h1).name, "| get(h2) =", get(h2).name);
    console.log("same cell:", h1 === h2 && h2 === cell);
}

main();
