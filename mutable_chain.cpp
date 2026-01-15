/**
 * @file mutable_chain.cpp
 * @brief List-Wrapper Based Reference Management Pattern - Modern C++ Implementation
 *
 * This module demonstrates a reference addressing mechanism using shared_ptr
 * wrappers (Ref<T>) to enable safe mutation of chain structures during iteration.
 *
 * @section pattern PATTERN EXPLANATION
 *
 * Instead of storing direct pointers between nodes:
 * @code
 *     node->next = other_node;              // Direct pointer
 * @endcode
 *
 * We use shared_ptr wrappers as an indirection layer:
 * @code
 *     node->next = make_shared<Ref<Node>>(other_node);  // Wrapped reference
 * @endcode
 *
 * @section benefits BENEFITS OF REF<T> WRAPPERS
 *
 * 1. **SHARED REFERENCE INDIRECTION**:
 *    - Multiple nodes can hold a shared_ptr to the same Ref wrapper
 *    - Updating the wrapper's ptr member affects all referencing nodes
 *    - When we change what's inside the wrapper, all holders see the update
 *
 * 2. **MUTABLE IDENTITY**:
 *    - The Ref wrapper's identity (address) stays the same
 *    - Only its contents (ptr member) change during mutations
 *    - All shared_ptrs to the wrapper remain valid after restructuring
 *
 * 3. **SAFE DELETION DURING ITERATION**:
 *    - When deleting a node, we update wrapper contents, not reassign pointers
 *    - The iterator's reference to predecessor->next still points to same Ref
 *    - That Ref now contains the successor, so iteration continues seamlessly
 *    - No explicit "deleted" flag or iterator invalidation handling needed
 *
 * 4. **AUTOMATIC MEMORY MANAGEMENT**:
 *    - shared_ptr provides automatic reference counting
 *    - Nodes are freed when no longer referenced
 *    - No manual memory management required
 *
 * @section deletion DELETION MECHANISM
 *
 * When we delete node B from chain A -> B -> C:
 *
 * Before deletion:
 * @code
 *     A.next = Ref{B}     B.prev = Ref{A}
 *     B.next = Ref{C}     C.prev = Ref{B}
 * @endcode
 *
 * Deletion updates Ref contents (not the Ref objects themselves):
 * @code
 *     A.next->ptr = C     // Same Ref object, now points to C
 *     C.prev->ptr = A     // Same Ref object, now points to A
 * @endcode
 *
 * After deletion (Ref objects unchanged, contents updated):
 * @code
 *     A.next = Ref{C}     // Same Ref object identity
 *     C.prev = Ref{A}     // Same Ref object identity
 * @endcode
 *
 * @section iteration ITERATION WITH DELETION
 *
 * The iterator holds a pointer to the current node.
 * When that node is deleted:
 * - The node's next Ref still exists and now points to successor
 * - operator++ follows next->ptr which leads to the correct successor
 * - Iteration continues seamlessly without special handling
 *
 * @section usecases USE CASES
 *
 * - Doubly-linked lists with concurrent deletion
 * - Graph structures with mutable node connections
 * - Priority queues with dynamic reordering
 * - Undo/redo systems where connections are restructured
 * - Scene graphs in graphics engines where nodes are reparented
 * - Any structure requiring safe modification during traversal
 *
 * @section stl STL COMPLIANCE
 *
 * MutableList<T> models the following STL concepts:
 * - Container (begin, end, size, empty, clear)
 * - ReversibleContainer (rbegin, rend)
 * - SequenceContainer (front, back, push_back, push_front, pop_back, pop_front)
 * - AllocatorAwareContainer (allocator_type template parameter)
 *
 * @author Based on Python reference implementation
 * @version 2.0 (Modern C++ rewrite with templates and STL compliance)
 */

#include <iostream>
#include <iterator>
#include <memory>
#include <utility>

// ============================================================================
// GENERIC COMPONENTS
// ============================================================================

/**
 * @brief Templatized wrapper providing indirection layer - the core pattern.
 *
 * Ref<T> wraps a shared_ptr<T> to enable updating what a reference points to
 * without changing the Ref object itself. Multiple shared_ptr<Ref<T>> can
 * point to the same Ref, and updating Ref::ptr affects all of them.
 *
 * @tparam T The type being referenced (typically a node type)
 *
 * @code
 *     auto ref = make_shared<Ref<Node>>(nodeA);
 *     auto ref_alias = ref;  // Both point to same Ref
 *     ref->ptr = nodeB;      // Both ref and ref_alias now see nodeB
 * @endcode
 */
template <typename T>
struct Ref {
    std::shared_ptr<T> ptr;  ///< The actual pointer, mutable for indirection
    explicit Ref(std::shared_ptr<T> p) : ptr(std::move(p)) {}
};

/**
 * @brief Generic doubly-linked node holding a value of type T.
 *
 * Uses Ref<ListNode<T>> for next/prev links to enable safe mutation.
 * The value is stored directly in the node (not wrapped).
 *
 * @tparam T The value type stored in this node
 */
template <typename T>
struct ListNode {
    using RefType = Ref<ListNode<T>>;      ///< The Ref type for this node
    using RefPtr = std::shared_ptr<RefType>; ///< Shared pointer to Ref

    RefPtr next;   ///< Forward link (wrapped for safe mutation)
    RefPtr prev;   ///< Backward link (wrapped for safe mutation)
    T value;       ///< The stored value

    /// Default constructor for sentinel nodes (no value initialization)
    ListNode() = default;

    /// Value constructor with perfect forwarding
    template <typename... Args>
    explicit ListNode(Args&&... args) : value(std::forward<Args>(args)...) {}
};

/**
 * @brief STL-compliant doubly-linked list with safe deletion during iteration.
 *
 * MutableList<T> is a container that allows elements to be erased while
 * iterating without invalidating the iterator. This is achieved through
 * the Ref<T> indirection pattern.
 *
 * Models: Container, ReversibleContainer, SequenceContainer (partial)
 *
 * @tparam T The element type
 * @tparam Allocator The allocator type (default: std::allocator<T>)
 *
 * @code
 *     MutableList<std::string> list{"a", "b", "c"};
 *
 *     // Safe deletion during iteration - just like Python!
 *     for (auto it = list.begin(); it != list.end(); ++it) {
 *         if (*it == "b") {
 *             list.erase(it);  // No special handling needed!
 *         }
 *     }
 * @endcode
 */
template <typename T, typename Allocator = std::allocator<T>>
class MutableList {
public:
    // ========================================================================
    // STL TYPE ALIASES
    // ========================================================================
    using value_type = T;                    ///< Element type
    using allocator_type = Allocator;        ///< Allocator type
    using size_type = std::size_t;           ///< Unsigned integer type for sizes
    using difference_type = std::ptrdiff_t;  ///< Signed integer type for differences
    using reference = value_type&;           ///< Reference to element
    using const_reference = const value_type&;  ///< Const reference to element
    using pointer = typename std::allocator_traits<Allocator>::pointer;
    using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;

private:
    using Node = ListNode<T>;
    using NodePtr = std::shared_ptr<Node>;
    using RefType = typename Node::RefType;
    using RefPtr = typename Node::RefPtr;

    NodePtr head_;     ///< Sentinel node before first element
    NodePtr tail_;     ///< Sentinel node after last element
    size_type size_ = 0;  ///< Number of elements

    /**
     * @brief Create bidirectional link between two nodes using Ref wrappers.
     *
     * This is the key linking operation. It creates NEW Ref objects for
     * each direction. The Ref wrappers enable later mutation.
     */
    static void link(const NodePtr& a, const NodePtr& b) {
        a->next = std::make_shared<RefType>(b);
        b->prev = std::make_shared<RefType>(a);
    }

    /// Create an empty sentinel node
    static NodePtr makeSentinel() {
        return std::make_shared<Node>();
    }

public:
    // ========================================================================
    // ITERATOR
    // ========================================================================

    /**
     * @brief Bidirectional iterator with const/non-const support.
     *
     * The iterator stores a shared_ptr to the current node. When dereferenced,
     * it returns a reference to the node's value. Incrementing follows
     * current_->next->ptr, which works correctly even if the current node
     * was just erased (because next->ptr has been updated to skip it).
     *
     * @tparam IsConst If true, produces const references
     */
    template <bool IsConst>
    class IteratorImpl {
        friend class MutableList;
        NodePtr current_;

    public:
        // STL iterator traits
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = std::conditional_t<IsConst, const T, T>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        IteratorImpl() = default;
        explicit IteratorImpl(NodePtr node) : current_(std::move(node)) {}

        /// Allow conversion from non-const to const iterator
        template <bool WasConst, typename = std::enable_if_t<IsConst && !WasConst>>
        IteratorImpl(const IteratorImpl<WasConst>& other) : current_(other.current_) {}

        reference operator*() const { return current_->value; }
        pointer operator->() const { return &current_->value; }

        /// Pre-increment: advance to next node via Ref indirection
        IteratorImpl& operator++() {
            current_ = current_->next->ptr;
            return *this;
        }

        IteratorImpl operator++(int) {
            IteratorImpl tmp = *this;
            ++(*this);
            return tmp;
        }

        /// Pre-decrement: move to previous node via Ref indirection
        IteratorImpl& operator--() {
            current_ = current_->prev->ptr;
            return *this;
        }

        IteratorImpl operator--(int) {
            IteratorImpl tmp = *this;
            --(*this);
            return tmp;
        }

        bool operator==(const IteratorImpl& o) const { return current_ == o.current_; }
        bool operator!=(const IteratorImpl& o) const { return current_ != o.current_; }

        /// Get underlying node pointer (for erase operations)
        NodePtr node() const { return current_; }
    };

    using iterator = IteratorImpl<false>;
    using const_iterator = IteratorImpl<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================

    /// Default constructor: creates empty list with sentinel nodes
    MutableList() : head_(makeSentinel()), tail_(makeSentinel()) {
        link(head_, tail_);
    }

    /// Initializer list constructor
    MutableList(std::initializer_list<T> init) : MutableList() {
        for (const auto& v : init) push_back(v);
    }

    /// Copy constructor
    MutableList(const MutableList& other) : MutableList() {
        for (const auto& v : other) push_back(v);
    }

    /// Move constructor
    MutableList(MutableList&& other) noexcept
        : head_(std::move(other.head_)),
          tail_(std::move(other.tail_)),
          size_(other.size_) {
        other.head_ = makeSentinel();
        other.tail_ = makeSentinel();
        link(other.head_, other.tail_);
        other.size_ = 0;
    }

    /// Copy/move assignment (copy-and-swap idiom)
    MutableList& operator=(MutableList other) {
        swap(other);
        return *this;
    }

    /// Swap contents with another list
    void swap(MutableList& other) noexcept {
        std::swap(head_, other.head_);
        std::swap(tail_, other.tail_);
        std::swap(size_, other.size_);
    }

    // ========================================================================
    // ITERATORS
    // ========================================================================

    iterator begin() { return iterator(head_->next->ptr); }
    iterator end() { return iterator(tail_); }
    const_iterator begin() const { return const_iterator(head_->next->ptr); }
    const_iterator end() const { return const_iterator(tail_); }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }

    reverse_iterator rbegin() { return reverse_iterator(end()); }
    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
    const_reverse_iterator crbegin() const { return rbegin(); }
    const_reverse_iterator crend() const { return rend(); }

    // ========================================================================
    // CAPACITY
    // ========================================================================

    /// Check if the list is empty
    [[nodiscard]] bool empty() const { return size_ == 0; }

    /// Get the number of elements
    [[nodiscard]] size_type size() const { return size_; }

    // ========================================================================
    // ELEMENT ACCESS
    // ========================================================================

    /// Access the first element
    reference front() { return head_->next->ptr->value; }
    const_reference front() const { return head_->next->ptr->value; }

    /// Access the last element
    reference back() { return tail_->prev->ptr->value; }
    const_reference back() const { return tail_->prev->ptr->value; }

    // ========================================================================
    // MODIFIERS
    // ========================================================================

    /// Remove all elements
    void clear() {
        link(head_, tail_);
        size_ = 0;
    }

    /**
     * @brief Construct element in-place at the end.
     * @param args Arguments forwarded to T's constructor
     * @return Reference to the inserted element
     */
    template <typename... Args>
    reference emplace_back(Args&&... args) {
        auto node = std::make_shared<Node>(std::forward<Args>(args)...);
        auto last = tail_->prev->ptr;
        link(last, node);
        link(node, tail_);
        ++size_;
        return node->value;
    }

    /// Add element to the end (copy)
    void push_back(const T& value) { emplace_back(value); }

    /// Add element to the end (move)
    void push_back(T&& value) { emplace_back(std::move(value)); }

    /**
     * @brief Construct element in-place at the beginning.
     * @param args Arguments forwarded to T's constructor
     * @return Reference to the inserted element
     */
    template <typename... Args>
    reference emplace_front(Args&&... args) {
        auto node = std::make_shared<Node>(std::forward<Args>(args)...);
        auto first = head_->next->ptr;
        link(head_, node);
        link(node, first);
        ++size_;
        return node->value;
    }

    /// Add element to the beginning (copy)
    void push_front(const T& value) { emplace_front(value); }

    /// Add element to the beginning (move)
    void push_front(T&& value) { emplace_front(std::move(value)); }

    /// Remove the last element
    void pop_back() {
        if (!empty()) erase(iterator(tail_->prev->ptr));
    }

    /// Remove the first element
    void pop_front() {
        if (!empty()) erase(begin());
    }

    /**
     * @brief Erase element at iterator position.
     *
     * **SAFE DURING ITERATION**: This operation updates Ref wrapper contents
     * rather than reassigning pointers. The predecessor's next Ref now points
     * to the successor, so any iterator that was pointing to the erased node
     * will correctly advance to the successor on the next increment.
     *
     * @param pos Iterator to the element to remove
     * @return Iterator to the element following the removed element
     *
     * @code
     *     for (auto it = list.begin(); it != list.end(); ++it) {
     *         if (should_remove(*it)) {
     *             list.erase(it);  // Just delete - iterator continues correctly!
     *         }
     *     }
     * @endcode
     */
    iterator erase(iterator pos) {
        auto node = pos.node();
        auto pred = node->prev->ptr;
        auto succ = node->next->ptr;
        // CRITICAL: Update Ref CONTENTS, not reassign pointers
        // This is what enables safe deletion during iteration
        pred->next->ptr = succ;  // A->B->C becomes A->C
        succ->prev->ptr = pred;  // C's prev updated from B to A
        --size_;
        return iterator(succ);
    }

    /**
     * @brief Erase element by node pointer (alternative API).
     *
     * Can be used when you have direct access to the node rather than
     * an iterator. Also safe during iteration.
     *
     * @param node The node to remove
     */
    void erase_node(const NodePtr& node) {
        auto pred = node->prev->ptr;
        auto succ = node->next->ptr;
        pred->next->ptr = succ;
        succ->prev->ptr = pred;
        --size_;
    }
};

/// Free function swap for ADL (Argument-Dependent Lookup)
template <typename T, typename A>
void swap(MutableList<T, A>& a, MutableList<T, A>& b) noexcept { a.swap(b); }

// ============================================================================
// EXAMPLE USAGE
// ============================================================================

int main() {
    // Works with any type - using initializer list
    MutableList<std::string> list{"data1!", "data2!", "data3!"};

    // Forward iteration with safe deletion - Python-style, no special handling!
    std::cout << "Forward iteration (delete data1):\n";
    for (auto it = list.begin(); it != list.end(); ++it) {
        if (*it == "data1!") {
            std::cout << "  removing: " << *it << '\n';
            list.erase(it);  // Just delete! Iterator continues correctly.
        } else {
            std::cout << "  - " << *it << '\n';
        }
    }

    // Reverse iteration using STL reverse_iterator
    std::cout << "\nReverse iteration:\n";
    for (auto it = list.rbegin(); it != list.rend(); ++it) {
        std::cout << "  - " << *it << '\n';
    }

    // Range-based for loop
    std::cout << "\nRange-based for:\n";
    for (const auto& s : list) {
        std::cout << "  - " << s << '\n';
    }

    // STL-style accessors
    std::cout << "\nSize: " << list.size() << '\n';
    std::cout << "Front: " << list.front() << '\n';
    std::cout << "Back: " << list.back() << '\n';

    // Works with other types
    MutableList<int> numbers{1, 2, 3, 4, 5};
    std::cout << "\nIntegers: ";
    for (int n : numbers) std::cout << n << ' ';
    std::cout << '\n';
}
