# Mutable Chain Technique: Safe Deletion During Iteration

## The Problem

You're iterating through a linked chain and need to delete a node mid-iteration. But deleting it breaks your iterator because it invalidates references. You can't just reassign pointers—other parts of your code still hold references to the old structure.

**What if you could delete nodes safely without any special handling?**

## The Solution

This project demonstrates an elegant reference management pattern using **list wrappers as an indirection layer**. Instead of:

```python
node["next"] = other_node  # Direct reference
```

We use:

```python
node["next"] = [other_node]  # Wrapped in a list
```

This simple change unlocks something powerful: **you can modify what nodes point to without breaking existing references**—because the list object itself never changes, only its contents.

## Why This Matters

- ✅ **Delete nodes during iteration** without crashing or skipping elements
- ✅ **No special iterator logic needed**—standard loops just work
- ✅ **Multiple pointers stay valid**—because they reference the list, not the contents
- ✅ **Elegant and minimal**—no flags, no callbacks, no complexity

## Quick Example

```python
# Build chain: start -> data1 -> data2 -> data3 -> end
link_to_and_from(start, data1)
link_to_and_from(data1, data2)
link_to_and_from(data2, data3)
link_to_and_from(data3, end)

# Safely delete nodes while iterating
for node in link_iterator(start):
    if should_delete(node):
        link_data_remove(node)  # Safe!
    else:
        process(node)
```

## Use Cases

- Doubly-linked lists with concurrent deletion
- Graph structures with mutable node connections
- Priority queues with dynamic reordering
- Undo/redo systems
- Scene graphs in graphics engines

## Implementations

This project includes three implementations of the mutable chain pattern:

- **Python**: [mutable_chain.py](mutable_chain.py) - Original implementation with detailed documentation
- **C**: [mutable_chain.c](mutable_chain.c) - C implementation using pointer wrappers
- **C++**: [mutable_chain.cpp](mutable_chain.cpp) - Modern C++ implementation using `shared_ptr`

## Building and Running

### Python
```bash
python mutable_chain.py
```

### C/C++ (using CMake)
```bash
mkdir build && cd build
cmake ..
cmake --build .
./bin/mutable_chain_c    # Run C version
./bin/mutable_chain_cpp  # Run C++ version
```

See [BUILD.md](BUILD.md) for detailed build instructions and alternative compilation methods.

## See the Details

Check [mutable_chain.py](mutable_chain.py) for the full implementation and deep dive into how the deletion mechanism works.
