/*
 * List-Wrapper Based Reference Management Pattern - C Implementation
 *
 * This module demonstrates a reference addressing mechanism using pointer
 * wrappers to enable safe mutation of chain structures during iteration.
 *
 * PATTERN EXPLANATION:
 * ====================
 *
 * Instead of storing direct references:
 *     a_link->to = a_data              // Direct reference
 *
 * We use pointer wrappers as an indirection layer:
 *     a_link->to = &wrapper             // Wrapped reference
 *     wrapper->node = a_data
 *
 * BENEFITS OF POINTER WRAPPERS:
 * ==============================
 *
 * 1. SHARED REFERENCE INDIRECTION:
 *    - Multiple nodes can hold a pointer to the same wrapper
 *    - Updating the wrapper's node affects all referencing nodes
 *    - Example: Both node A and node B hold pointers to the same wrapper
 *    - When we change what's inside the wrapper, both A and B see the update
 *
 * 2. MUTABLE IDENTITY:
 *    - The wrapper's identity stays the same, only its contents change
 *    - All pointers to the wrapper remain valid even after mutation
 *    - No need to update multiple reference points when restructuring
 *
 * 3. SAFE DELETION DURING ITERATION:
 *    - When deleting a node, we update wrapper contents, not reassign pointers
 *    - The iterator's reference to predecessor->to still points to same wrapper
 *    - That wrapper now contains the successor, so iteration continues seamlessly
 *    - No explicit "deleted" flag needed
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Forward declarations */
typedef struct NodeWrapper NodeWrapper;
typedef struct Node Node;

/* Wrapper structure - the indirection layer */
struct NodeWrapper {
    Node *node;
};

/* Node structure - represents data in the chain */
struct Node {
    NodeWrapper *to;       /* Forward reference wrapper */
    NodeWrapper *from;     /* Backward reference wrapper */
    char *data;            /* Node data (NULL for terminal/initial) */
    bool is_initial;       /* Flag for initial node */
    bool is_terminal;      /* Flag for terminal node */
};

/*
 * Create a new wrapper pointing to a node
 */
NodeWrapper* create_wrapper(Node *node) {
    NodeWrapper *wrapper = (NodeWrapper*)malloc(sizeof(NodeWrapper));
    if (!wrapper) {
        fprintf(stderr, "Memory allocation failed for wrapper\n");
        exit(1);
    }
    wrapper->node = node;
    return wrapper;
}

/*
 * Create a new node with data
 */
Node* create_node(const char *data) {
    Node *node = (Node*)malloc(sizeof(Node));
    if (!node) {
        fprintf(stderr, "Memory allocation failed for node\n");
        exit(1);
    }
    node->to = NULL;
    node->from = NULL;
    node->is_initial = false;
    node->is_terminal = false;
    
    if (data) {
        node->data = (char*)malloc(strlen(data) + 1);
        if (!node->data) {
            fprintf(stderr, "Memory allocation failed for data\n");
            exit(1);
        }
        strcpy(node->data, data);
    } else {
        node->data = NULL;
    }
    
    return node;
}

/*
 * Create initial node
 */
Node* create_initial_node() {
    Node *node = create_node(NULL);
    node->is_initial = true;
    return node;
}

/*
 * Create terminal node
 */
Node* create_terminal_node() {
    Node *node = create_node(NULL);
    node->is_terminal = true;
    return node;
}

/*
 * Create a bidirectional link between two nodes using wrappers.
 *
 * Args:
 *     a_link: Source node
 *     a_data: Target node
 *
 * Result:
 *     a_link->to = wrapper pointing to a_data
 *     a_data->from = wrapper pointing to a_link
 *
 * The wrappers enable shared reference indirection. When we update
 * the wrapper's contents later (during deletion), all pointers to these
 * wrappers automatically see the updated reference without needing reassignment.
 */
void link_to_and_from(Node *a_link, Node *a_data) {
    a_link->to = create_wrapper(a_data);
    a_data->from = create_wrapper(a_link);
}

/*
 * Remove a node from the chain by restructuring connections.
 *
 * This is the key operation that demonstrates the power of wrappers.
 * Instead of reassigning pointers, we UPDATE the wrapper contents.
 *
 * Args:
 *     a_link: Node to remove from chain
 *
 * Operation:
 *     - predecessor->to->node = successor    // Update wrapper CONTENTS
 *     - successor->from->node = predecessor  // Update wrapper CONTENTS
 *
 * Result:
 *     - The wrapper objects (predecessor->to and successor->from) remain
 *       the same object references, but their contents have changed
 *     - Any code holding references to these wrappers sees the updated chain
 *     - The iterator automatically continues to the successor without
 *       needing to detect or handle the deletion
 */
void link_data_remove(Node *a_link) {
    /* Capture the nodes we're connecting */
    Node *predecessor = a_link->from->node;
    Node *successor = a_link->to->node;
    
    /* CRITICAL: Update wrapper CONTENTS, not reassign pointers */
    /* This keeps the wrapper objects stable while changing what they point to */
    predecessor->to->node = successor;    /* A->B->C becomes A->C (update A's wrapper) */
    successor->from->node = predecessor;  /* C's predecessor updates from B to A */
}

/*
 * Iterate through the chain starting from a_link.
 *
 * Args:
 *     a_link: Starting node for iteration
 *     reverse: If true, iterate backwards using "from" references.
 *              If false, iterate forwards using "to" references.
 *     callback: Function to call for each node in the iteration
 *     user_data: User data to pass to the callback
 *
 * Handles deletion gracefully because of wrapper indirection:
 * - When a yielded node is deleted, a_link->to or a_link->from still exists
 * - But its contents have been updated to point to the successor/predecessor
 * - The next loop iteration automatically follows the updated reference
 *
 * Safe deletion pattern:
 *     void process_node(Node *node, void *data) {
 *         if (should_delete(node)) {
 *             link_data_remove(node);  // Safe! Iterator handles it
 *         }
 *     }
 *     link_iterator(start, false, process_node, NULL);
 */
void link_iterator(Node *a_link, bool reverse, void (*callback)(Node*, void*), void *user_data) {
    Node *current = a_link;
    
    while (true) {
        /* Get the next node from the current wrapper reference */
        Node *next_node = reverse ? current->from->node : current->to->node;
        
        /* Check for termination */
        if ((reverse && next_node->is_initial) || (!reverse && next_node->is_terminal)) {
            break;
        }
        
        /* Call the callback with the next node */
        callback(next_node, user_data);
        
        /* Move to the next node */
        /* If next_node was just deleted, it's orphaned, but the wrapper */
        /* has been updated to point to the successor/predecessor */
        current = next_node;
    }
}

/*
 * Free a node and its data
 */
void free_node(Node *node) {
    if (node) {
        if (node->data) {
            free(node->data);
        }
        if (node->to) {
            free(node->to);
        }
        if (node->from) {
            free(node->from);
        }
        free(node);
    }
}

/* ============================================================================
 * EXAMPLE USAGE
 * ============================================================================ */

/* Callback for forward iteration that removes data1 */
void forward_callback(Node *node, void *user_data) {
    if (node->data && strcmp(node->data, "data1!") == 0) {
        printf("  removing : %s\n", node->data);
        link_data_remove(node);
    } else {
        printf("  -  %s\n", node->data ? node->data : "(null)");
    }
}

/* Callback for reverse iteration */
void reverse_callback(Node *node, void *user_data) {
    printf("  -  %s\n", node->data ? node->data : "(null)");
}

int main() {
    /* Create nodes */
    Node *link1 = create_initial_node();
    Node *data1 = create_node("data1!");
    Node *data2 = create_node("data2!");
    Node *data3 = create_node("data3!");
    Node *terminal = create_terminal_node();
    
    /* Build chain: link1 -> data1 -> data2 -> data3 -> terminal */
    link_to_and_from(link1, data1);
    link_to_and_from(data1, data2);
    link_to_and_from(data2, data3);
    link_to_and_from(data3, terminal);
    
    /* Forward iteration: delete data1 mid-iteration */
    printf("Forward iteration (delete data1):\n");
    link_iterator(link1, false, forward_callback, NULL);
    
    /* Reverse iteration: terminal -> link1 */
    printf("\nReverse iteration (terminal -> link1):\n");
    link_iterator(terminal, true, reverse_callback, NULL);
    
    /* Cleanup */
    free_node(link1);
    free_node(data1);
    free_node(data2);
    free_node(data3);
    free_node(terminal);
    
    return 0;
}
