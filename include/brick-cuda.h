/**
 * @file
 * @brief For using bricks with CUDA
 */

#ifndef BRICK_BRICK_CUDA_H
#define BRICK_BRICK_CUDA_H

#include <brick.h>
#include <cuda_runtime.h>

/**
 * @brief Check the return of CUDA calls, do nothing during release build
 */
#ifndef NDEBUG
#define cudaCheck(x) x
#else

#include <cstdio>

#define cudaCheck(x) _cudaCheck(x, #x ,__FILE__, __LINE__)
#endif

/// Internal for #cudaCheck(x)
template<typename T>
void _cudaCheck(T e, const char *func, const char *call, const int line) {
  if (e != cudaSuccess) {
    printf("\"%s\" at %d in %s\n\treturned %d\n-> %s\n", func, line, call, (int) e, cudaGetErrorString(e));
    exit(EXIT_FAILURE);
  }
}

/**
 * @brief Moving BrickInfo to GPU (allocate new)
 * @tparam dims implicit when used with bInfo argument
 * @param bInfo BrickInfo to copy from host
 * @param kind Currently must be cudaMemcpyHostToDevice
 * @return a new BrickInfo struct allocated on the GPU
 */
template<unsigned dims>
BrickInfo<dims> movBrickInfo(BrickInfo<dims> &bInfo, cudaMemcpyKind kind) {
  // Make a copy
  BrickInfo<dims> ret = bInfo;
  unsigned size = bInfo.nbricks * static_power<3, dims>::value * sizeof(unsigned);
  cudaCheck(cudaMalloc(&ret.adj, size));
  cudaCheck(cudaMemcpy(ret.adj, bInfo.adj, size, kind));
  return ret;
}

/**
 * @brief Moving BrickStorage to GPU (allocate new)
 * @param bStorage BrickStorage to copy from host
 * @param kind Currently must be cudaMemcpyHostToDevice
 * @return a new BrickStorage struct allocated on the GPU
 */
inline BrickStorage movBrickStorage(BrickStorage &bStorage, cudaMemcpyKind kind) {
  // Make a copy
  BrickStorage ret = bStorage;
  unsigned size = bStorage.step * bStorage.chunks * sizeof(bElem);
  bElem *devptr;
  cudaCheck(cudaMalloc(&devptr, size));
  cudaCheck(cudaMemcpy(devptr, bStorage.dat.get(), size, kind));
  ret.dat = std::shared_ptr<bElem>(devptr, [](bElem *p) { cudaFree(p); });
  return ret;
}

#include "dev_shl.h"

#endif //BRICK_BRICK_CUDA_H
