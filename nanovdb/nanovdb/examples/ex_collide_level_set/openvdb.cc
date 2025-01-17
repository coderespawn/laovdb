// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

#if defined(NANOVDB_USE_OPENVDB)

#include <cmath>
#include <chrono>

#include <openvdb/openvdb.h>
#include <openvdb/math/Ray.h>

#include <nanovdb/util/CudaDeviceBuffer.h>
#include <nanovdb/util/NanoToOpenVDB.h>

#include "common.h"

#if defined(NANOVDB_USE_CUDA)
using BufferT = nanovdb::CudaDeviceBuffer;
#else
using BufferT = nanovdb::HostBuffer;
#endif

void runOpenVDB(nanovdb::GridHandle<BufferT>& handle, int numIterations, int numPoints, BufferT& positionBuffer, BufferT& velocityBuffer)
{
    using GridT = laovdb::FloatGrid;
    using CoordT = laovdb::Coord;
    using RealT = float;
    using Vec3T = laovdb::math::Vec3<RealT>;
    using RayT = laovdb::math::Ray<RealT>;

    auto srcGrid = nanovdb::nanoToOpenVDB(handle);
    std::cout << "Exporting to OpenVDB grid[" << srcGrid->getName() << "]...\n";

    auto h_grid = (GridT*)srcGrid.get();

    // Not yet implemented...
}

#endif
