# List-Wrapper Based Reference Management Pattern
#
# This module demonstrates a reference addressing mechanism using array wrappers
# to enable safe mutation of chain structures during iteration.
#
# PATTERN EXPLANATION:
# ====================
#
# Instead of storing direct references:
#     a_link[:to] = a_data              # Direct reference
#
# We use array wrappers as an indirection layer:
#     a_link[:to] = [a_data]            # Wrapped reference
#
# BENEFITS OF ARRAY WRAPPERS:
# ===========================
#
# 1. SHARED REFERENCE INDIRECTION:
#    - Multiple nodes can hold a reference to the same array
#    - Updating the array's contents affects all referencing nodes
#    - Example: Both node A and node B hold a[:to] and b[:to] pointing to [node_C]
#    - When we change what's inside the array, both A and B see the update
#
# 2. MUTABLE IDENTITY:
#    - The array object's identity stays the same, only its contents change
#    - All pointers to the array remain valid even after mutation
#    - No need to update multiple reference points when restructuring
#
# 3. SAFE DELETION DURING ITERATION:
#    - When deleting a node, we update array contents, not reassign references
#    - The iterator's reference to predecessor[:to] still points to the same array
#    - That array now contains the successor, so iteration continues seamlessly
#    - No explicit "deleted" flag needed
#
# DELETION MECHANISM:
# ===================
#
# When we delete node B from chain A -> B -> C:
#
# Before deletion:
#     A[:to] = [B]
#     B[:from] = [A]
#     B[:to] = [C]
#     C[:from] = [B]
#
# Deletion calls: link_data_remove(B)
#     B[:from][0][:to][0] = B[:to][0]  # Update A's array contents to C
#     B[:to][0][:from][0] = B[:from][0]  # Update C's array contents to A
#
# After deletion (A's array object is the SAME, contents changed):
#     A[:to] = [C]                       # Array object unchanged, contents updated
#     B[:from] = [A]                     # B is orphaned but irrelevant
#     B[:to] = [C]                       # B is orphaned but irrelevant
#     C[:from] = [A]
#
# ITERATION WITH DELETION:
# ========================
#
# The iterator holds a reference to A[:to], which is an array.
# Even if that array's contents change mid-iteration, the reference remains valid.
# The iterator simply follows the chain using the updated array contents.
#
# USE CASES:
# ==========
#
# 1. DOUBLY-LINKED LISTS with concurrent deletion
# 2. GRAPH STRUCTURES with mutable node connections
# 3. PRIORITY QUEUES with dynamic reordering
# 4. UNDO/REDO SYSTEMS where connections are restructured
# 5. SCENE GRAPHS in graphics engines where nodes are reparented
#
# Any structure where you need to:
# - Modify connections/references
# - Maintain multiple pointers to connection points
# - Avoid updating all referencing pointers when restructuring

# Create a bidirectional link between two nodes using array wrappers.
#
# @param a_link [Hash] Source node
# @param a_data [Hash] Target node
#
# Result:
#     a_link[:to] = [a_data]        # Array wrapper for forward reference
#     a_data[:from] = [a_link]      # Array wrapper for backward reference
#
# The array wrappers enable shared reference indirection. When we update
# the array's contents later (during deletion), all pointers to these arrays
# automatically see the updated reference without needing reassignment.
def link_to_and_from(a_link, a_data)
  a_link[:to] = [a_data]
  a_data[:from] = [a_link]
end

# Remove a node from the chain by restructuring connections.
#
# This is the key operation that demonstrates the power of array wrappers.
# Instead of reassigning references, we UPDATE the array contents.
#
# @param a_link [Hash] Node to remove from chain
#
# Operation:
#     - predecessor[:to][0] = successor    # Update array CONTENTS
#     - successor[:from][0] = predecessor  # Update array CONTENTS
#
# Result:
#     - The array objects (predecessor[:to] and successor[:from]) remain
#       the same object references, but their contents have changed
#     - Any code holding references to these arrays sees the updated chain
#     - The iterator automatically continues to the successor without
#       needing to detect or handle the deletion
def link_data_remove(a_link)
  # Capture the nodes we're connecting
  predecessor = a_link[:from][0]
  successor = a_link[:to][0]

  # CRITICAL: Update array CONTENTS, not reassign references
  # This keeps the array objects stable while changing what they point to
  predecessor[:to][0] = successor    # A->B->C becomes A->C (update A's array)
  successor[:from][0] = predecessor  # C's predecessor updates from B to A
end

# Enumerator that iterates through the chain starting from a_link.
#
# @param a_link [Hash] Starting node for iteration
# @param reverse [Boolean] If true, iterate backwards using :from references.
#                          If false (default), iterate forwards using :to references.
#
# Handles deletion gracefully because of array-wrapper indirection:
# - When a yielded node is deleted, a_link[:to] or a_link[:from] still exists
# - But its contents have been updated to point to the successor/predecessor
# - The next loop iteration automatically follows the updated reference
#
# @yield [Hash] Each non-terminal node in the chain
#
# Safe deletion pattern:
#     link_iterator(start).each do |node|
#       link_data_remove(node) if should_delete(node)  # Safe! Iterator handles it
#     end
def link_iterator(a_link, reverse: false)
  Enumerator.new do |yielder|
    direction_key = reverse ? :from : :to
    terminal_check = reverse ? :initial : :terminal

    loop do
      # Get the next node from the current array reference
      next_node = a_link[direction_key][0]

      # Check for termination
      break if next_node.key?(terminal_check)

      yielder << next_node

      # Move to the next node
      # If next_node was just deleted, it's orphaned, but a_link[direction_key][0]
      # has been updated to point to the successor/predecessor, so this works seamlessly
      a_link = next_node
    end
  end
end

# ============================================================================
# EXAMPLE USAGE
# ============================================================================

if __FILE__ == $PROGRAM_NAME
  link1 = { initial: true }
  data1 = { data: "data1!" }
  data2 = { data: "data2!" }
  data3 = { data: "data3!" }
  terminal = { terminal: true }

  # Build chain: link1 -> data1 -> data2 -> data3 -> terminal
  link_to_and_from(link1, data1)
  link_to_and_from(data1, data2)
  link_to_and_from(data2, data3)
  link_to_and_from(data3, terminal)

  # Forward iteration: delete data1 mid-iteration
  puts "Forward iteration (delete data1):"
  link_iterator(link1).each do |i_data|
    if i_data[:data] == "data1!"
      puts "  removing : #{i_data[:data]}"
      link_data_remove(i_data)
    else
      puts "  -  #{i_data[:data]}"
    end
  end

  # Reverse iteration: terminal -> link1
  puts "\nReverse iteration (terminal -> link1):"
  link_iterator(terminal, reverse: true).each do |i_data|
    puts "  -  #{i_data[:data]}"
  end
end
