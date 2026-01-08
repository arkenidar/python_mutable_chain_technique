"""
List-Wrapper Based Reference Management Pattern

This module demonstrates a reference addressing mechanism using list wrappers
to enable safe mutation of chain structures during iteration.

PATTERN EXPLANATION:
====================

Instead of storing direct references:
    a_link["to"] = a_data              # Direct reference

We use list wrappers as an indirection layer:
    a_link["to"] = [a_data]            # Wrapped reference

BENEFITS OF LIST WRAPPERS:
==========================

1. SHARED REFERENCE INDIRECTION:
   - Multiple nodes can hold a reference to the same list
   - Updating the list's contents affects all referencing nodes
   - Example: Both node A and node B hold a["to"] and b["to"] pointing to [node_C]
   - When we change what's inside the list, both A and B see the update

2. MUTABLE IDENTITY:
   - The list object's identity stays the same, only its contents change
   - All pointers to the list remain valid even after mutation
   - No need to update multiple reference points when restructuring

3. SAFE DELETION DURING ITERATION:
   - When deleting a node, we update list contents, not reassign references
   - The iterator's reference to predecessor["to"] still points to the same list
   - That list now contains the successor, so iteration continues seamlessly
   - No explicit "deleted" flag needed

DELETION MECHANISM:
===================

When we delete node B from chain A -> B -> C:

Before deletion:
    A["to"] = [B]
    B["from"] = [A]
    B["to"] = [C]
    C["from"] = [B]

Deletion calls: link_data_remove(B)
    B["from"][0]["to"][0] = B["to"][0]  # Update A's list contents to C
    B["to"][0]["from"][0] = B["from"][0]  # Update C's list contents to A

After deletion (A's list object is the SAME, contents changed):
    A["to"] = [C]                       # List object unchanged, contents updated
    B["from"] = [A]                     # B is orphaned but irrelevant
    B["to"] = [C]                       # B is orphaned but irrelevant
    C["from"] = [A]

ITERATION WITH DELETION:
========================

The iterator holds a reference to A["to"], which is a list.
Even if that list's contents change mid-iteration, the reference remains valid.
The iterator simply follows the chain using the updated list contents.

USE CASES:
==========

1. DOUBLY-LINKED LISTS with concurrent deletion
2. GRAPH STRUCTURES with mutable node connections
3. PRIORITY QUEUES with dynamic reordering
4. UNDO/REDO SYSTEMS where connections are restructured
5. SCENE GRAPHS in graphics engines where nodes are reparented

Any structure where you need to:
- Modify connections/references
- Maintain multiple pointers to connection points
- Avoid updating all referencing pointers when restructuring
"""

def link_to_and_from(a_link, a_data):
    """
    Create a bidirectional link between two nodes using list wrappers.
    
    Args:
        a_link: Source node (dict)
        a_data: Target node (dict)
    
    Result:
        a_link["to"] = [a_data]        # List wrapper for forward reference
        a_data["from"] = [a_link]      # List wrapper for backward reference
    
    The list wrappers enable shared reference indirection. When we update
    the list's contents later (during deletion), all pointers to these lists
    automatically see the updated reference without needing reassignment.
    """
    a_link["to"] = [a_data]
    a_data["from"] = [a_link]


def link_data_remove(a_link):
    """
    Remove a node from the chain by restructuring connections.
    
    This is the key operation that demonstrates the power of list wrappers.
    Instead of reassigning references, we UPDATE the list contents.
    
    Args:
        a_link: Node to remove from chain
    
    Operation:
        - predecessor["to"][0] = successor    # Update list CONTENTS
        - successor["from"][0] = predecessor  # Update list CONTENTS
    
    Result:
        - The list objects (predecessor["to"] and successor["from"]) remain
          the same object references, but their contents have changed
        - Any code holding references to these lists sees the updated chain
        - The iterator automatically continues to the successor without
          needing to detect or handle the deletion
    """
    # Capture the nodes we're connecting
    predecessor = a_link["from"][0]
    successor = a_link["to"][0]
    
    # CRITICAL: Update list CONTENTS, not reassign references
    # This keeps the list objects stable while changing what they point to
    predecessor["to"][0] = successor    # A->B->C becomes A->C (update A's list)
    successor["from"][0] = predecessor  # C's predecessor updates from B to A


def link_iterator(a_link, reverse=False):
    """
    Generator that iterates through the chain starting from a_link.
    
    Args:
        a_link: Starting node for iteration
        reverse: If True, iterate backwards using "from" references.
                 If False (default), iterate forwards using "to" references.
    
    Handles deletion gracefully because of list-wrapper indirection:
    - When a yielded node is deleted, a_link["to"] or a_link["from"] still exists
    - But its contents have been updated to point to the successor/predecessor
    - The next loop iteration automatically follows the updated reference
    
    Yields:
        Each non-terminal node in the chain
    
    Safe deletion pattern:
        for node in link_iterator(start):
            if should_delete(node):
                link_data_remove(node)  # Safe! Iterator handles it
    """
    direction_key = "from" if reverse else "to"
    terminal_check = "initial" if reverse else "terminal"
    
    while True:
        # Get the next node from the current list reference
        next_node = a_link[direction_key][0]
        
        # Check for termination
        if terminal_check in next_node:
            break
        
        yield next_node
        
        # Move to the next node
        # If next_node was just deleted, it's orphaned, but a_link[direction_key][0]
        # has been updated to point to the successor/predecessor, so this works seamlessly
        a_link = next_node


if __name__ == "__main__":
    # ============================================================================
    # EXAMPLE USAGE
    # ============================================================================

    link1 = { "initial": True }
    data1 = { "data": "data1!" }
    data2 = { "data": "data2!" }
    data3 = { "data": "data3!" }
    terminal = { "terminal": True }

    # Build chain: link1 -> data1 -> data2 -> data3 -> terminal
    link_to_and_from(link1, data1)
    link_to_and_from(data1, data2)
    link_to_and_from(data2, data3)
    link_to_and_from(data3, terminal)

    # Forward iteration: delete data1 mid-iteration
    print("Forward iteration (delete data1):")
    for i_data in link_iterator(link1):
        if i_data["data"] == "data1!":
            print("  removing :", i_data["data"])
            link_data_remove(i_data)
        else:
            print("  - ", i_data["data"])

    # Reverse iteration: terminal -> link1
    print("\nReverse iteration (terminal -> link1):")
    for i_data in link_iterator(terminal, reverse=True):
        print("  - ", i_data["data"])