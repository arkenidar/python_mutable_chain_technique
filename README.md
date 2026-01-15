# Mutable Chain Technique: Safe Deletion During Iteration

A reference management pattern using **wrapper indirection** to enable safe node deletion while iterating through linked structures.

## The Problem

You're iterating through a linked chain and need to delete a node mid-iteration. But deleting it breaks your iterator because it invalidates references. You can't just reassign pointers—other parts of your code still hold references to the old structure.

**What if you could delete nodes safely without any special handling?**

## The Solution

Instead of storing direct references between nodes:

```python
node["next"] = other_node  # Direct reference
```

We wrap them in a mutable container (list/array):

```python
node["next"] = [other_node]  # Wrapped in a list
```

This simple change unlocks something powerful: **you can modify what nodes point to without breaking existing references**—because the wrapper object itself never changes, only its contents.

## How It Works

When deleting node B from chain `A -> B -> C`:

```
Before:  A.next = [B]    B.next = [C]
         ↓               ↓
         B               C

After:   A.next = [C]    (same wrapper object, new contents)
         ↓
         C
```

The wrapper `A.next` is the **same object**—only its contents changed. Any code holding a reference to that wrapper automatically sees the updated target.

## Why This Matters

- ✅ **Delete nodes during iteration** without crashing or skipping elements
- ✅ **No special iterator logic needed**—standard loops just work
- ✅ **Multiple pointers stay valid**—they reference the wrapper, not the contents
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
        link_data_remove(node)  # Safe! Iterator continues correctly
    else:
        process(node)
```

## Implementations

| Language | File | Wrapper Type | Notes |
|----------|------|--------------|-------|
| **Python** | [mutable_chain.py](mutable_chain.py) | `list` | Reference implementation with full documentation |
| **C** | [mutable_chain.c](mutable_chain.c) | `struct` pointer | Manual memory management |
| **C++** | [mutable_chain.cpp](mutable_chain.cpp) | `Ref<T>` template | STL-compliant `MutableList<T>` container |
| **Lua** | [mutable_chain.lua](mutable_chain.lua) | `table` | Idiomatic Lua with LuaDoc |
| **JavaScript** | [mutable_chain.js](mutable_chain.js) | `Array` | Minimal ES6 with generators |
| **JavaScript** | [mutable_chain.mjs](mutable_chain.mjs) | `Array` | Full ES Modules version |
| **Ruby** | [mutable_chain.rb](mutable_chain.rb) | `Array` | Idiomatic Ruby |

### C++ Highlights

The C++ implementation provides an STL-compliant generic container:

```cpp
MutableList<std::string> list{"a", "b", "c"};

// Safe deletion during iteration
for (auto it = list.begin(); it != list.end(); ++it) {
    if (*it == "b") {
        it = list.erase(it);
        --it;  // Compensate for loop's ++it
    }
}

// Works with any type
MutableList<int> numbers{1, 2, 3, 4, 5};
```

Features:
- `Ref<T>` template for generic indirection
- Full iterator support (`begin`, `end`, `rbegin`, `rend`)
- STL container interface (`push_back`, `emplace_back`, `front`, `back`, `size`, `empty`)
- Copy/move semantics, initializer lists
- Doxygen documentation

## Use Cases

- **Doubly-linked lists** with concurrent deletion
- **Graph structures** with mutable node connections  
- **Priority queues** with dynamic reordering
- **Undo/redo systems** where connections are restructured
- **Scene graphs** in graphics engines where nodes are reparented
- **Event systems** where handlers can unsubscribe during dispatch

## Building and Running

### Python
```bash
python mutable_chain.py
```

### Lua (with LuaJIT)
```bash
luajit mutable_chain.lua
```

### JavaScript (Node.js)
```bash
node mutable_chain.js
# or ES Modules version:
node mutable_chain.mjs
```

### Ruby
```bash
ruby mutable_chain.rb
```

### C/C++ (using CMake)
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
./bin/mutable_chain_c    # Run C version
./bin/mutable_chain_cpp  # Run C++ version
```

See [BUILD.md](BUILD.md) for detailed build instructions.

## Project Structure

```
├── mutable_chain.py      # Python (reference implementation)
├── mutable_chain.c       # C
├── mutable_chain.cpp     # Modern C++ (STL-compliant)
├── mutable_chain.lua     # Lua
├── mutable_chain.js      # JavaScript (CommonJS)
├── mutable_chain.mjs     # JavaScript (ES Modules)
├── mutable_chain.rb      # Ruby
├── CMakeLists.txt        # CMake build for C/C++
├── BUILD.md              # Detailed build instructions
└── README.md             # This file
```

## License

MIT
