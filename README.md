# A C++ implementation of make's core features
This project implements
* the parsing logic of make (`src/makefile-parser.cpp`, `src/variables.cpp`) with a focus on simplifying control flow,
* a general purpose concurrent task execution engine (`src/task-graph.cpp`) that uses modern C++ synchronization primitives, and
* the detailed error handling of make.

Testing is done with gtest and a Python test harness.

To try out this implementation of make on a Linux machine,
```
# Build
mkdir build
cd build
cmake ..
make -j$(nproc) 
cd ..

# Run make on an example target
./build/MiniMake -f tests/test.mk -j 10 parallel

# Run make a number of makefiles and targets used for testing
./tests/run_build_tests

# Run gtests
./build/makefile-parser-tests
./build/variables-tests
./build/string-ops-tests
./build/task-graph-tests
```