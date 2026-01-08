/*
 * List-Wrapper Based Reference Management Pattern - C++ Implementation
 *
 * This module demonstrates a reference addressing mechanism using shared_ptr
 * wrappers to enable safe mutation of chain structures during iteration.
 *
 * PATTERN EXPLANATION:
 * ====================
 *
 * Instead of storing direct references:
 *     a_link->to = a_data              // Direct reference
 *
 * We use shared_ptr wrappers as an indirection layer:
 *     a_link->to = make_shared<Wrapper>(a_data)
 *
 * BENEFITS OF SHARED_PTR WRAPPERS:
 * =================================
 *
 * 1. SHARED REFERENCE INDIRECTION:
 *    - Multiple nodes can hold a shared_ptr to the same wrapper
 *    - Updating the wrapper's node affects all referencing nodes
 *    - Example: Both node A and node B hold shared_ptrs to the same wrapper
 *    - When we change what's inside the wrapper, both A and B see the update
 *
 * 2. MUTABLE IDENTITY:
 *    - The wrapper's identity stays the same, only its contents change
 *    - All shared_ptrs to the wrapper remain valid even after mutation
 *    - No need to update multiple reference points when restructuring
 *
 * 3. SAFE DELETION DURING ITERATION:
 *    - When deleting a node, we update wrapper contents, not reassign pointers
 *    - The iterator's reference to predecessor->to still points to same wrapper
 *    - That wrapper now contains the successor, so iteration continues seamlessly
 *    - No explicit "deleted" flag needed
 *
 * 4. AUTOMATIC MEMORY MANAGEMENT:
 *    - shared_ptr provides automatic reference counting
 *    - No manual memory management required
 */

#include <iostream>
#include <memory>
#include <string>
#include <functional>

/* Forward declaration */
class Node;

/* Wrapper class - the indirection layer */
class NodeWrapper {
public:
    std::shared_ptr<Node> node;
    
    explicit NodeWrapper(std::shared_ptr<Node> n) : node(n) {}
};

/* Node class - represents data in the chain */
class Node {
public:
    std::shared_ptr<NodeWrapper> to;       /* Forward reference wrapper */
    std::shared_ptr<NodeWrapper> from;     /* Backward reference wrapper */
    std::string data;                       /* Node data */
    bool is_initial;                        /* Flag for initial node */
    bool is_terminal;                       /* Flag for terminal node */
    
    /* Constructor for data nodes */
    explicit Node(const std::string& d = "") 
        : data(d), is_initial(false), is_terminal(false) {}
    
    /* Static factory for initial node */
    static std::shared_ptr<Node> create_initial() {
        auto node = std::make_shared<Node>();
        node->is_initial = true;
        return node;
    }
    
    /* Static factory for terminal node */
    static std::shared_ptr<Node> create_terminal() {
        auto node = std::make_shared<Node>();
        node->is_terminal = true;
        return node;
    }
    
    /* Static factory for data node */
    static std::shared_ptr<Node> create_data(const std::string& data) {
        return std::make_shared<Node>(data);
    }
};

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
void link_to_and_from(std::shared_ptr<Node> a_link, std::shared_ptr<Node> a_data) {
    a_link->to = std::make_shared<NodeWrapper>(a_data);
    a_data->from = std::make_shared<NodeWrapper>(a_link);
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
void link_data_remove(std::shared_ptr<Node> a_link) {
    /* Capture the nodes we're connecting */
    auto predecessor = a_link->from->node;
    auto successor = a_link->to->node;
    
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
 *
 * Handles deletion gracefully because of wrapper indirection:
 * - When a yielded node is deleted, a_link->to or a_link->from still exists
 * - But its contents have been updated to point to the successor/predecessor
 * - The next loop iteration automatically follows the updated reference
 *
 * Safe deletion pattern:
 *     link_iterator(start, false, [](auto node) {
 *         if (should_delete(node)) {
 *             link_data_remove(node);  // Safe! Iterator handles it
 *         }
 *     });
 */
void link_iterator(std::shared_ptr<Node> a_link, bool reverse, 
                  std::function<void(std::shared_ptr<Node>)> callback) {
    auto current = a_link;
    
    while (true) {
        /* Get the next node from the current wrapper reference */
        auto next_node = reverse ? current->from->node : current->to->node;
        
        /* Check for termination */
        if ((reverse && next_node->is_initial) || (!reverse && next_node->is_terminal)) {
            break;
        }
        
        /* Call the callback with the next node */
        callback(next_node);
        
        /* Move to the next node */
        /* If next_node was just deleted, it's orphaned, but the wrapper */
        /* has been updated to point to the successor/predecessor */
        current = next_node;
    }
}

/* ============================================================================
 * EXAMPLE USAGE
 * ============================================================================ */

int main() {
    /* Create nodes */
    auto link1 = Node::create_initial();
    auto data1 = Node::create_data("data1!");
    auto data2 = Node::create_data("data2!");
    auto data3 = Node::create_data("data3!");
    auto terminal = Node::create_terminal();
    
    /* Build chain: link1 -> data1 -> data2 -> data3 -> terminal */
    link_to_and_from(link1, data1);
    link_to_and_from(data1, data2);
    link_to_and_from(data2, data3);
    link_to_and_from(data3, terminal);
    
    /* Forward iteration: delete data1 mid-iteration */
    std::cout << "Forward iteration (delete data1):" << std::endl;
    link_iterator(link1, false, [](auto node) {
        if (node->data == "data1!") {
            std::cout << "  removing : " << node->data << std::endl;
            link_data_remove(node);
        } else {
            std::cout << "  -  " << node->data << std::endl;
        }
    });
    
    /* Reverse iteration: terminal -> link1 */
    std::cout << "\nReverse iteration (terminal -> link1):" << std::endl;
    link_iterator(terminal, true, [](auto node) {
        std::cout << "  -  " << node->data << std::endl;
    });
    
    return 0;
}
