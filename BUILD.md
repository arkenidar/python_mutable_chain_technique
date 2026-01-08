# Build Instructions

This project contains three implementations of the mutable chain technique:
- **Python**: `mutable_chain.py`
- **C**: `mutable_chain.c`
- **C++**: `mutable_chain.cpp`

## Building with CMake

### Prerequisites
- CMake 3.10 or higher
- C compiler (gcc, clang, or MSVC)
- C++ compiler supporting C++14 (g++, clang++, or MSVC)

### Build Steps

#### On Windows (using Visual Studio)

```bash
# Create build directory
mkdir build
cd build

# Generate Visual Studio project files
cmake ..

# Build the project
cmake --build . --config Release

# Run the executables
bin\Release\mutable_chain_c.exe
bin\Release\mutable_chain_cpp.exe
```

#### On Windows (using MinGW or MSYS2)

```bash
# Create build directory
mkdir build
cd build

# Generate Makefiles
cmake .. -G "MinGW Makefiles"

# Build the project
cmake --build .

# Run the executables
bin\mutable_chain_c.exe
bin\mutable_chain_cpp.exe
```

#### On Linux/macOS

```bash
# Create build directory
mkdir build
cd build

# Generate Makefiles
cmake ..

# Build the project
make

# Run the executables
./bin/mutable_chain_c
./bin/mutable_chain_cpp
```

### Alternative: Direct Compilation (without CMake)

#### C version
```bash
# Windows (MSVC)
cl /W4 mutable_chain.c /Fe:mutable_chain_c.exe

# Linux/macOS (gcc)
gcc -Wall -Wextra -std=c11 mutable_chain.c -o mutable_chain_c

# Run
./mutable_chain_c  # Linux/macOS
mutable_chain_c.exe  # Windows
```

#### C++ version
```bash
# Windows (MSVC)
cl /W4 /EHsc mutable_chain.cpp /Fe:mutable_chain_cpp.exe

# Linux/macOS (g++)
g++ -Wall -Wextra -std=c++14 mutable_chain.cpp -o mutable_chain_cpp

# Run
./mutable_chain_cpp  # Linux/macOS
mutable_chain_cpp.exe  # Windows
```

## Running the Python version

```bash
python mutable_chain.py
```

## Expected Output

All three implementations should produce the same output:

```
Forward iteration (delete data1):
  removing : data1!
  -  data2!
  -  data3!

Reverse iteration (terminal -> link1):
  -  data3!
  -  data2!
```

Note: data1 is successfully deleted during iteration, and the iterator continues seamlessly to data2, demonstrating the power of the mutable chain technique.

## CMake Build Options

```bash
# Debug build
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release build (optimized)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Specify compiler
cmake .. -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
```

## Cleaning Build Files

```bash
# Remove build directory
rm -rf build  # Linux/macOS
rmdir /s /q build  # Windows
```
