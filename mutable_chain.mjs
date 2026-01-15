/**
 * List-Wrapper Based Reference Management Pattern
 *
 * This module demonstrates a reference addressing mechanism using array wrappers
 * to enable safe mutation of chain structures during iteration.
 *
 * PATTERN EXPLANATION:
 * ====================
 *
 * Instead of storing direct references:
 *     aLink.to = aData;              // Direct reference
 *
 * We use array wrappers as an indirection layer:
 *     aLink.to = [aData];            // Wrapped reference
 *
 * BENEFITS OF ARRAY WRAPPERS:
 * ===========================
 *
 * 1. SHARED REFERENCE INDIRECTION:
 *    - Multiple nodes can hold a reference to the same array
 *    - Updating the array's contents affects all referencing nodes
 *    - Example: Both node A and node B hold a.to and b.to pointing to [nodeC]
 *    - When we change what's inside the array, both A and B see the update
 *
 * 2. MUTABLE IDENTITY:
 *    - The array object's identity stays the same, only its contents change
 *    - All pointers to the array remain valid even after mutation
 *    - No need to update multiple reference points when restructuring
 *
 * 3. SAFE DELETION DURING ITERATION:
 *    - When deleting a node, we update array contents, not reassign references
 *    - The iterator's reference to predecessor.to still points to the same array
 *    - That array now contains the successor, so iteration continues seamlessly
 *    - No explicit "deleted" flag needed
 *
 * DELETION MECHANISM:
 * ===================
 *
 * When we delete node B from chain A -> B -> C:
 *
 * Before deletion:
 *     A.to = [B]
 *     B.from = [A]
 *     B.to = [C]
 *     C.from = [B]
 *
 * Deletion calls: linkDataRemove(B)
 *     B.from[0].to[0] = B.to[0];    // Update A's array contents to C
 *     B.to[0].from[0] = B.from[0];  // Update C's array contents to A
 *
 * After deletion (A's array object is the SAME, contents changed):
 *     A.to = [C]                     // Array object unchanged, contents updated
 *     B.from = [A]                   // B is orphaned but irrelevant
 *     B.to = [C]                     // B is orphaned but irrelevant
 *     C.from = [A]
 *
 * ITERATION WITH DELETION:
 * ========================
 *
 * The iterator holds a reference to A.to, which is an array.
 * Even if that array's contents change mid-iteration, the reference remains valid.
 * The iterator simply follows the chain using the updated array contents.
 *
 * USE CASES:
 * ==========
 *
 * 1. DOUBLY-LINKED LISTS with concurrent deletion
 * 2. GRAPH STRUCTURES with mutable node connections
 * 3. PRIORITY QUEUES with dynamic reordering
 * 4. UNDO/REDO SYSTEMS where connections are restructured
 * 5. SCENE GRAPHS in graphics engines where nodes are reparented
 *
 * Any structure where you need to:
 * - Modify connections/references
 * - Maintain multiple pointers to connection points
 * - Avoid updating all referencing pointers when restructuring
 */

/**
 * Create a bidirectional link between two nodes using array wrappers.
 *
 * @param {Object} aLink - Source node
 * @param {Object} aData - Target node
 *
 * Result:
 *     aLink.to = [aData]        // Array wrapper for forward reference
 *     aData.from = [aLink]      // Array wrapper for backward reference
 *
 * The array wrappers enable shared reference indirection. When we update
 * the array's contents later (during deletion), all pointers to these arrays
 * automatically see the updated reference without needing reassignment.
 */
export function linkToAndFrom(aLink, aData) {
    aLink.to = [aData];
    aData.from = [aLink];
}

/**
 * Remove a node from the chain by restructuring connections.
 *
 * This is the key operation that demonstrates the power of array wrappers.
 * Instead of reassigning references, we UPDATE the array contents.
 *
 * @param {Object} aLink - Node to remove from chain
 *
 * Operation:
 *     predecessor.to[0] = successor;    // Update array CONTENTS
 *     successor.from[0] = predecessor;  // Update array CONTENTS
 *
 * Result:
 *     - The array objects (predecessor.to and successor.from) remain
 *       the same object references, but their contents have changed
 *     - Any code holding references to these arrays sees the updated chain
 *     - The iterator automatically continues to the successor without
 *       needing to detect or handle the deletion
 */
export function linkDataRemove(aLink) {
    // Capture the nodes we're connecting
    const predecessor = aLink.from[0];
    const successor = aLink.to[0];

    // CRITICAL: Update array CONTENTS, not reassign references
    // This keeps the array objects stable while changing what they point to
    predecessor.to[0] = successor;    // A->B->C becomes A->C (update A's array)
    successor.from[0] = predecessor;  // C's predecessor updates from B to A
}

/**
 * Generator that iterates through the chain starting from aLink.
 *
 * @param {Object} aLink - Starting node for iteration
 * @param {boolean} [reverse=false] - If true, iterate backwards using "from" references.
 *                                    If false (default), iterate forwards using "to" references.
 *
 * Handles deletion gracefully because of array-wrapper indirection:
 * - When a yielded node is deleted, aLink.to or aLink.from still exists
 * - But its contents have been updated to point to the successor/predecessor
 * - The next loop iteration automatically follows the updated reference
 *
 * @yields {Object} Each non-terminal node in the chain
 *
 * Safe deletion pattern:
 *     for (const node of linkIterator(start)) {
 *         if (shouldDelete(node)) {
 *             linkDataRemove(node);  // Safe! Iterator handles it
 *         }
 *     }
 */
export function* linkIterator(aLink, reverse = false) {
    const directionKey = reverse ? "from" : "to";
    const terminalCheck = reverse ? "initial" : "terminal";

    while (true) {
        // Get the next node from the current array reference
        const nextNode = aLink[directionKey][0];

        // Check for termination
        if (terminalCheck in nextNode) {
            break;
        }

        yield nextNode;

        // Move to the next node
        // If nextNode was just deleted, it's orphaned, but aLink[directionKey][0]
        // has been updated to point to the successor/predecessor, so this works seamlessly
        aLink = nextNode;
    }
}

// ============================================================================
// EXAMPLE USAGE
// ============================================================================

function main() {
    const link1 = { initial: true };
    const data1 = { data: "data1!" };
    const data2 = { data: "data2!" };
    const data3 = { data: "data3!" };
    const terminal = { terminal: true };

    // Build chain: link1 -> data1 -> data2 -> data3 -> terminal
    linkToAndFrom(link1, data1);
    linkToAndFrom(data1, data2);
    linkToAndFrom(data2, data3);
    linkToAndFrom(data3, terminal);

    // Forward iteration: delete data1 mid-iteration
    console.log("Forward iteration (delete data1):");
    for (const iData of linkIterator(link1)) {
        if (iData.data === "data1!") {
            console.log("  removing:", iData.data);
            linkDataRemove(iData);
        } else {
            console.log("  -", iData.data);
        }
    }

    // Reverse iteration: terminal -> link1
    console.log("\nReverse iteration (terminal -> link1):");
    for (const iData of linkIterator(terminal, true)) {
        console.log("  -", iData.data);
    }
}

// Run if executed directly (ESM)
main();
