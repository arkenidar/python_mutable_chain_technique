# frozen_string_literal: true

# List-Wrapper Based Reference Management Pattern
#
# Demonstrates a reference addressing mechanism using array wrappers
# to enable safe mutation of chain structures during iteration.
#
# Instead of direct references (a_link[:to] = a_data), we use array wrappers
# (a_link[:to] = [a_data]) as an indirection layer. This enables:
#
# 1. SHARED REFERENCE INDIRECTION - Multiple nodes reference the same array;
#    updating contents affects all referencing nodes
# 2. MUTABLE IDENTITY - Array identity stays the same, only contents change
# 3. SAFE DELETION DURING ITERATION - Update array contents, not references
#
# Use cases: doubly-linked lists, graph structures, priority queues,
# undo/redo systems, scene graphs - any structure needing safe mutation.

module MutableChain
  # Sentinel modules for type-safe terminal detection
  module Initial; end
  module Terminal; end

  class Node
    attr_accessor :data, :to, :from

    def initialize(data = nil)
      @data = data
      @to = nil
      @from = nil
    end

    def initial? = data.is_a?(Module) && data == Initial
    def terminal? = data.is_a?(Module) && data == Terminal

    # Create bidirectional link: self -> other
    def link_to(other)
      @to = [other]
      other.from = [self]
      other
    end

    # Remove this node from chain by updating neighbors' array contents
    def remove
      predecessor = @from[0]
      successor = @to[0]

      # Update array CONTENTS, not references - this is the key insight
      predecessor.to[0] = successor
      successor.from[0] = predecessor
      self
    end

    def inspect
      case
      when initial? then "#<Node:initial>"
      when terminal? then "#<Node:terminal>"
      else "#<Node:#{@data.inspect}>"
      end
    end
  end

  class Chain
    include Enumerable

    def initialize
      @head = Node.new(Initial)
      @tail = Node.new(Terminal)
      @head.link_to(@tail)
    end

    # Append a new node to the chain
    def <<(data)
      node = data.is_a?(Node) ? data : Node.new(data)
      last_node = @tail.from[0]
      last_node.link_to(node)
      node.link_to(@tail)
      self
    end

    # Forward iteration - safe for deletion during iteration
    def each
      return to_enum(:each) unless block_given?

      current = @head
      while (next_node = current.to[0])
        break if next_node.terminal?
        yield next_node
        current = next_node
      end
    end

    # Reverse iteration
    def reverse_each
      return to_enum(:reverse_each) unless block_given?

      current = @tail
      while (prev_node = current.from[0])
        break if prev_node.initial?
        yield prev_node
        current = prev_node
      end
    end

    def to_a = map(&:data)
  end
end

# ============================================================================
# EXAMPLE USAGE
# ============================================================================

if __FILE__ == $PROGRAM_NAME
  include MutableChain

  chain = Chain.new
  chain << "data1!" << "data2!" << "data3!"

  # Forward iteration: delete data1 mid-iteration
  puts "Forward iteration (delete data1):"
  chain.each do |node|
    if node.data == "data1!"
      puts "  removing : #{node.data}"
      node.remove
    else
      puts "  -  #{node.data}"
    end
  end

  # Reverse iteration
  puts "\nReverse iteration:"
  chain.reverse_each { |node| puts "  -  #{node.data}" }
end
