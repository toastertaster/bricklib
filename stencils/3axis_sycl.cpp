//
// Created by Tuowen Zhao on 11/11/19.
//

#include <iostream>
#include <CL/sycl.hpp>
#include "brick-sycl.h"
#include "multiarray.h"
#include "brickcompare.h"
#include "stencils.h"

#define VSVEC "SYCL"
#define SYCL_SUBGROUP 16
#define VFOLD 2,8

using namespace cl::sycl;

cl::sycl::device *sycl_device;

void printInfo(cl::sycl::device &Device) {
  std::cout << "Using OpenCL " << (Device.is_cpu() ? "CPU" : "GPU")
            << " device {" << Device.get_info<cl::sycl::info::device::name>()
            << "} from {" << Device.get_info<cl::sycl::info::device::vendor>() << "}" << std::endl;

  auto dot_num_groups = Device.get_info<cl::sycl::info::device::max_compute_units>();
  auto dot_wgsize = Device.get_info<cl::sycl::info::device::max_work_group_size>();

  std::cout << "Compute units: " << dot_num_groups << std::endl;
  std::cout << "Workgroup size: " << dot_wgsize << std::endl;
}

void syclinit() {
  class NEOGPUDeviceSelector : public cl::sycl::device_selector {
  public:
    int operator()(const cl::sycl::device &Device) const override {
      const std::string DeviceName = Device.get_info<cl::sycl::info::device::name>();
      const std::string DeviceVendor = Device.get_info<cl::sycl::info::device::vendor>();

      return Device.is_gpu() && DeviceName.find("HD Graphics NEO") ? 1 : -1;
    }
  };

  sycl_device = new cl::sycl::device(NEOGPUDeviceSelector());
  printInfo(*sycl_device);
}

class BrickStencilFunctor {
public:
  BrickStencilFunctor(
      accessor<bElem, 1, access::mode::read_write, access::target::global_buffer> bDat,
      accessor<unsigned, 1, access::mode::read_write, access::target::global_buffer> adj,
      accessor<bElem, 1, access::mode::read, access::target::global_buffer> coeff,
      accessor<unsigned, 1, access::mode::read, access::target::global_buffer> bIdx,
      unsigned len) : bDat(bDat), adj(adj), coeff(coeff), bIdx(bIdx) {}

  [[cl::intel_reqd_sub_group_size(SYCL_SUBGROUP)]]
  void operator()(nd_item<1> WIid) {
    intel::sub_group SG = WIid.get_sub_group();
    int sglid = SG.get_local_id().get(0);

    oclbrick bIn = {&bDat[0], &adj[0], 1024};
    oclbrick bOut = {&bDat[512], &adj[0], 1024};
    for (unsigned i = WIid.get_group(0); i < len; i += WIid.get_group_range(0)) {
      unsigned b = bIdx[i];
      brick("7pt.py", VSVEC, (8, 8, 8), (VFOLD), b);
    }
  }

private:
  accessor<bElem, 1, access::mode::read_write, access::target::global_buffer> bDat;
  accessor<unsigned, 1, access::mode::read_write, access::target::global_buffer> adj;
  accessor<bElem, 1, access::mode::read, access::target::global_buffer> coeff;
  accessor<unsigned, 1, access::mode::read, access::target::global_buffer> bIdx;
  unsigned len;
};

void d3pt7() {
  unsigned *grid_ptr;

  auto bInfo = init_grid<3>(grid_ptr, {STRIDEB, STRIDEB, STRIDEB});
  auto grid = (unsigned (*)[STRIDEB][STRIDEB]) grid_ptr;

  bElem *in_ptr = randomArray({STRIDE, STRIDE, STRIDE});
  bElem *out_ptr = zeroArray({STRIDE, STRIDE, STRIDE});
  bElem(*arr_in)[STRIDE][STRIDE] = (bElem (*)[STRIDE][STRIDE]) in_ptr;
  bElem(*arr_out)[STRIDE][STRIDE] = (bElem (*)[STRIDE][STRIDE]) out_ptr;

  auto bSize = cal_size<BDIM>::value;
  auto bStorage = BrickStorage::allocate(bInfo.nbricks, bSize * 2);
  Brick<Dim<BDIM>, Dim<VFOLD>> bIn(&bInfo, bStorage, 0);
  Brick<Dim<BDIM>, Dim<VFOLD>> bOut(&bInfo, bStorage, bSize);

  cl::sycl::queue squeue(*sycl_device, {cl::sycl::property::queue::enable_profiling()});

  copyToBrick<3>({STRIDEG, STRIDEG, STRIDEG}, {PADDING, PADDING, PADDING}, {0, 0, 0}, in_ptr, grid_ptr, bIn);
  // Setup bricks for opencl
  buffer<bElem, 1> coeff_buf(coeff, range<1>(129));

  std::vector<unsigned> bIdx;

  for (long tk = GB; tk < STRIDEB - GB; ++tk)
    for (long tj = GB; tj < STRIDEB - GB; ++tj)
      for (long ti = GB; ti < STRIDEB - GB; ++ti)
        bIdx.push_back(grid[tk][tj][ti]);

  buffer<unsigned, 1> bIdx_buf(bIdx.data(), range<1>(bIdx.size()));

  size_t adj_size = bInfo.nbricks * 27;
  buffer<unsigned, 1> adj_buf((unsigned *) bInfo.adj, range<1>(adj_size));

  size_t bDat_size = bStorage.chunks * bStorage.step;
  buffer<bElem, 1> bDat_buf(bStorage.dat, range<1>(bDat_size));

  auto arr_func = [&arr_in, &arr_out]() -> void {
    _TILEFOR arr_out[k][j][i] = coeff[5] * arr_in[k + 1][j][i] + coeff[6] * arr_in[k - 1][j][i] +
                                coeff[3] * arr_in[k][j + 1][i] + coeff[4] * arr_in[k][j - 1][i] +
                                coeff[1] * arr_in[k][j][i + 1] + coeff[2] * arr_in[k][j][i - 1] +
                                coeff[0] * arr_in[k][j][i];
  };

  std::cout << "d3pt7" << std::endl;
  std::cout << "Arr: " << time_func(arr_func) << std::endl;

  nd_range<1> nworkitem(range<1>(1024 * SYCL_SUBGROUP), range<1>(SYCL_SUBGROUP));
  auto kernel = [&](handler &cgh) {
    BrickStencilFunctor bsf(bDat_buf.get_access<access::mode::read_write>(cgh),
                            adj_buf.get_access<access::mode::read_write>(cgh),
                            coeff_buf.get_access<access::mode::read>(cgh),
                            bIdx_buf.get_access<access::mode::read>(cgh),
                            bIdx.size());

    cgh.parallel_for<class brickStencilTrans>(nworkitem, bsf);
  };

  {
    // Sycl is too slow and failed with "Release"
    /*auto st_event = squeue.submit(kernel);
    int sycl_iter = 100;
    for (int i = 0; i < sycl_iter - 2; ++i)
      squeue.submit(kernel);*/
    auto ed_event = squeue.submit(kernel);
    ed_event.wait();
    double elapsed = ed_event.get_profiling_info<info::event_profiling::command_end>() -
                     ed_event.get_profiling_info<info::event_profiling::command_start>();
    elapsed *= 1e-9;
    std::cout << "Brick: " << elapsed << std::endl;

    if (!compareBrick<3>({N, N, N}, {PADDING, PADDING, PADDING}, {GZ, GZ, GZ}, out_ptr, grid_ptr, bOut)) {
      std::cout << "result mismatch!" << std::endl;
      // Identify mismatch
      for (long tk = GB; tk < STRIDEB - GB; ++tk)
        for (long tj = GB; tj < STRIDEB - GB; ++tj)
          for (long ti = GB; ti < STRIDEB - GB; ++ti) {
            auto b = grid[tk][tj][ti];
            for (long k = 0; k < TILE; ++k)
              for (long j = 0; j < TILE; ++j)
                for (long i = 0; i < TILE; ++i) {
                  auto aval = arr_out[tk * TILE + k + PADDING][tj * TILE + j + PADDING][ti * TILE + i + PADDING];
                  auto diff = abs(bOut[b][k][j][i] - aval);
                  auto sum = abs(bOut[b][k][j][i]) + abs(aval);
                  if (sum > 1e-6 && diff / sum > 1e-6)
                    std::cout << "mismatch at " << ti * TILE + i - TILE << " : " << tj * TILE + j - TILE << " : "
                              << tk * TILE + k - TILE << " : " << bOut[b][k][j][i] << std::endl;
                }
          }
    }
  }

  free(in_ptr);
  free(out_ptr);
  free(grid_ptr);
  free(bStorage.dat);
  free(bInfo.adj);
}
