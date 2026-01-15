--[[
    List-Wrapper Based Reference Management Pattern (Lua Implementation)

    This module demonstrates a reference addressing mechanism using table wrappers
    to enable safe mutation of chain structures during iteration.

    PATTERN EXPLANATION:
    ====================

    Instead of storing direct references:
        a_link.to = a_data              -- Direct reference

    We use table wrappers as an indirection layer:
        a_link.to = {a_data}            -- Wrapped reference (1-indexed in Lua)

    BENEFITS OF TABLE WRAPPERS:
    ===========================

    1. SHARED REFERENCE INDIRECTION:
       - Multiple nodes can hold a reference to the same table
       - Updating the table's contents affects all referencing nodes

    2. MUTABLE IDENTITY:
       - The table object's identity stays the same, only its contents change
       - All pointers to the table remain valid even after mutation

    3. SAFE DELETION DURING ITERATION:
       - When deleting a node, we update table contents, not reassign references
       - The iterator's closure captures the table reference, not its contents
       - No explicit "deleted" flag needed

    DELETION MECHANISM:
    ===================

    When we delete node B from chain A -> B -> C:

    Before deletion:
        A.to = {B}      B.from = {A}
        B.to = {C}      C.from = {B}

    After link_data_remove(B):
        A.to[1] = C     -- Same table, contents updated
        C.from[1] = A   -- Same table, contents updated

    USE CASES:
    ==========
    - Doubly-linked lists with concurrent deletion
    - Graph structures with mutable node connections
    - Priority queues with dynamic reordering
    - Scene graphs where nodes are reparented
]]

--- Create a bidirectional link between two nodes using table wrappers.
-- @param a_link Source node (table)
-- @param a_data Target node (table)
-- Result: a_link.to = {a_data}, a_data.from = {a_link}
local function link_to_and_from(a_link, a_data)
    a_link.to = { a_data }
    a_data.from = { a_link }
end

--- Remove a node from the chain by restructuring connections.
-- Updates table CONTENTS (not references) so iterators remain valid.
-- @param a_link Node to remove from chain
local function link_data_remove(a_link)
    local predecessor = a_link.from[1]
    local successor = a_link.to[1]
    -- CRITICAL: Update table CONTENTS, not reassign references
    predecessor.to[1] = successor   -- A->B->C becomes A->C
    successor.from[1] = predecessor -- C's predecessor updates from B to A
end

--- Generator that iterates through the chain.
-- Handles deletion gracefully because of table-wrapper indirection.
-- @param a_link Starting node for iteration
-- @param reverse If true, iterate backwards using "from" references
-- @return Iterator function yielding each non-terminal node
local function link_iterator(a_link, reverse)
    local direction_key = reverse and "from" or "to"
    local terminal_check = reverse and "initial" or "terminal"

    return function()
        local next_node = a_link[direction_key][1]
        if next_node[terminal_check] then
            return nil
        end
        a_link = next_node
        return next_node
    end
end

-- ============================================================================
-- EXAMPLE USAGE
-- ============================================================================
local link1 = { initial = true }
local data1 = { data = "data1!" }
local data2 = { data = "data2!" }
local data3 = { data = "data3!" }
local terminal = { terminal = true }

link_to_and_from(link1, data1)
link_to_and_from(data1, data2)
link_to_and_from(data2, data3)
link_to_and_from(data3, terminal)

print("Forward iteration (delete data1):")
for i_data in link_iterator(link1) do
    if i_data.data == "data1!" then
        print("  removing:", i_data.data)
        link_data_remove(i_data)
    else
        print("  -", i_data.data)
    end
end

print("\nReverse iteration (terminal -> link1):")
for i_data in link_iterator(terminal, true) do
    print("  -", i_data.data)
end
