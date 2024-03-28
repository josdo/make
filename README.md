# A C++ implementation of make's core features
This project implements
* make's parsing logic (`src/makefile-parser.cpp`, `src/variables.cpp`) with a focus on simplifying control flow,
* a general purpose concurrent task execution algorithm (`src/task-graph.cpp`) using modern C++ synchronization primitives, and
* make's detailed error messages.

Testing is done with gtest and a Python test harness.

To use it, run the following:
```
# Build
mkdir build
cd build
cmake ..
make -j$(nproc) 
cd ..

# Run make on an example target
./build/MiniMake -f tests/test.mk -j 10 parallel

# Run make on a number of makefiles and targets used for testing
./tests/run_build_tests

# Run gtests
./build/makefile-parser-tests
./build/variables-tests
./build/string-ops-tests
./build/task-graph-tests
```