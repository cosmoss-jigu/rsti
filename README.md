# Runtime Scope Type Integrity (RSTI)

This repository contains the source code for the ASPLOS'24 paper:

*"Enforcing C/C++ Type and Scope at Runtime for Control-Flow and Data-Flow Integrity"*
*Mohannad Ismail, Christopher Jelesnianski, Yeongjin Jang, Changwoo Min, and Wenjie Xiong*
*In Proceedings of the 29th ACM International Conference on Architectural Support for Programming Languages and Operating Systems (ASPLOS 2024)*

## Directory structure
```{.sh}
rsti
├── CMakeLists.txt    # CMake file for building RSTI
├── example           # Example code
├── llvm-project      # RSTI compiler
```


## Setup notes
RSTI uses a fork of Apple's LLVM, and thus this will only work on an Apple macOS machine with an ARM processor. This has been tested on an Apple Mac Mini M1.

## How to compile RSTI
```bash
$ mkdir build
$ cd build
$ cmake ..
$ make llvm-mac-all
```

#### Running examples
```bash
$ cd example
$ ./pactight_compile_c example
$ ./pactight_compile_c++ example_cpp
$ ./example
$ ./example_cpp
```
If you wish to use the RSTI compiler, please check the compile scripts in the example directory for all the flags necessary. You can copy the file and modify it as needed.
