/**
 * List-Wrapper Based Reference Management Pattern (JavaScript Implementation)
 *
 * Uses array wrappers as indirection layer for safe mutation during iteration.
 *
 * PATTERN: aLink.to = [aData] instead of aLink.to = aData
 *
 * BENEFITS:
 * - Shared reference indirection (update contents, all refs see it)
 * - Mutable identity (array object stays same, contents change)
 * - Safe deletion during iteration (no "deleted" flag needed)
 *
 * DELETION: A->B->C becomes A->C by updating A.to[0] = C, C.from[0] = A
 */

/** Create bidirectional link using array wrappers */
function linkToAndFrom(aLink, aData) {
    aLink.to = [aData];
    aData.from = [aLink];
}

/** Remove node by updating array CONTENTS (not references) */
function linkDataRemove(aLink) {
    const predecessor = aLink.from[0];
    const successor = aLink.to[0];
    predecessor.to[0] = successor;   // A->B->C becomes A->C
    successor.from[0] = predecessor; // C's predecessor: B -> A
}

/** Generator iterator - handles deletion gracefully */
function* linkIterator(aLink, reverse = false) {
    const directionKey = reverse ? "from" : "to";
    const terminalCheck = reverse ? "initial" : "terminal";
    while (true) {
        const nextNode = aLink[directionKey][0];
        if (nextNode[terminalCheck]) break;
        yield nextNode;
        aLink = nextNode;
    }
}

// ============================================================================
// EXAMPLE USAGE
// ============================================================================
const link1 = { initial: true };
const data1 = { data: "data1!" };
const data2 = { data: "data2!" };
const data3 = { data: "data3!" };
const terminal = { terminal: true };

linkToAndFrom(link1, data1);
linkToAndFrom(data1, data2);
linkToAndFrom(data2, data3);
linkToAndFrom(data3, terminal);

console.log("Forward iteration (delete data1):");
for (const iData of linkIterator(link1)) {
    if (iData.data === "data1!") {
        console.log("  removing:", iData.data);
        linkDataRemove(iData);
    } else {
        console.log("  -", iData.data);
    }
}

console.log("\nReverse iteration (terminal -> link1):");
for (const iData of linkIterator(terminal, true)) {
    console.log("  -", iData.data);
}
