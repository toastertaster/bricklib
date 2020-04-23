# Brick Layout for C++

**Artifacts for SC20**

[Readme of Brick Library](README.old.md)

## Organization

The specific communication methods are implemented as library functions and used by the corresponding stencil test.

* `include` and `src` contains the brick library headers and library files.
    * *Layout Optimization* is created with `BrickDecomp` from `include/bricks.run`
    * *Memory mapping* is created from `BrickDecomp` with `BrickDecomp::exchangeView`
* Included scaling test cases:
    * `weak` for weak scaling or strong scaling with one-level decomposition (one subdomain per rank)
    * `strong` for strong scaling with two-level decomposition (multiple fixed-sized subdomains per rank)

## Software dependencies

* GCC supporting *C++11*, recommend:
    * \>= 6.4.0
* CUDA, recommend:
    * \>= 10.0
* MPI
* Python 3.x
* *Optional* Intel Compiler and Intel OpenMP library, recommend:
    * \>= 2019

## How to Run

1. `mkdir build && cd build`
2. `cmake .. -DCMAKE_BUILD_TYPE=Release`
3. `make <test>`
4. run, for example, `weak-cpu`:
    1. `cd weak`
    2. `./cpu`

Tests includes:

* CPU
    * `weak-cpu`: `weak/cpu` One-level decomposition with MemMap
    * `strong-cpu`: `strong/cpu` Two-level decomposition with MemMap
* GPU
    * `weak-cuda`: `weak/cuda` One-level decomposition with Layout optimization and CUDA-Aware MPI
    * `weak-cuda-mmap`: `weak/cuda-mmap` One-level decomposition with MemMap
    * `strong-cuda-mmap`: `strong/cuda-mmap` Two-level decomposition with MemMap

Use `<test> -h` to list all parameters to the test.

### Change Stencil

Edit `stencils/fake.h`:[LN35](stencils/fake.h#L35) to

* 7-point: *MPI_7PT*
* 125-point: *MPI_125PT*
