// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

#if defined(NANOVDB_USE_OPENVDB)

#include <cmath>
#include <chrono>

#include <openvdb/openvdb.h>
#include <openvdb/math/Ray.h>
#include <openvdb/tools/LevelSetSphere.h>

#include <nanovdb/util/CudaDeviceBuffer.h>
#include <nanovdb/util/NanoToOpenVDB.h>

#include "common.h"

#if defined(NANOVDB_USE_CUDA)
using BufferT = nanovdb::CudaDeviceBuffer;
#else
using BufferT = nanovdb::HostBuffer;
#endif

void runOpenVDB(nanovdb::GridHandle<BufferT>& handle, int numIterations, int width, int height, BufferT& imageBuffer)
{
    using GridT = laovdb::FloatGrid;
    using CoordT = laovdb::Coord;
    using RealT = float;
    using Vec3T = laovdb::math::Vec3<RealT>;
    using RayT = laovdb::math::Ray<RealT>;

    auto srcGrid = nanovdb::nanoToOpenVDB(handle);
    std::cout << "Exporting to OpenVDB grid[" << srcGrid->getName() << "]...\n";

    auto h_grid = (GridT*)srcGrid.get();

    float* h_outImage = reinterpret_cast<float*>(imageBuffer.data());

    auto  indexBBox = h_grid->evalActiveVoxelBoundingBox();
    auto  gridXform = h_grid->transformPtr();
    auto  worldBBox = gridXform->indexToWorld(indexBBox);
    float wBBoxDimZ = (float)worldBBox.extents()[2] * 2;
    Vec3T wBBoxCenter = Vec3T(worldBBox.min() + worldBBox.extents() * 0.5f);

    RayGenOp<Vec3T> rayGenOp(wBBoxDimZ, wBBoxCenter);
    CompositeOp     compositeOp;

    laovdb::CoordBBox treeIndexBbox;
    treeIndexBbox = h_grid->evalActiveVoxelBoundingBox();
    std::cout << "Bounds: " << treeIndexBbox << std::endl;

    auto renderOp = [width, height, rayGenOp, compositeOp, treeIndexBbox] __hostdev__(int start, int end, float* image, const GridT* grid) {
        // get an accessor.
        auto acc = grid->getAccessor();

        for (int i = start; i < end; ++i) {
            Vec3T rayEye;
            Vec3T rayDir;
            rayGenOp(i, width, height, rayEye, rayDir);
            // generate ray.
            RayT wRay(rayEye, rayDir);
            // transform the ray to the grid's index-space.
            RayT iRay = wRay.worldToIndex(*grid);
            // clip to bounds.
            if (iRay.clip(treeIndexBbox) == false) {
                compositeOp(image, i, width, height, 0.0f, 0.0f);
                return;
            }
            // integrate...
            const float dt = 0.5f;
            float       transmittance = 1.0f;
            for (float t = iRay.t0(); t < iRay.t1(); t += dt) {
                float sigma = acc.getValue(CoordT::floor(iRay(t))) * 0.1f;
                transmittance *= 1.0f - sigma * dt;
            }
            // write transmittance.
            compositeOp(image, i, width, height, 0.0f, 1.0f - transmittance);
        }
    };

    {
        float durationAvg = 0;
        for (int i = 0; i < numIterations; ++i) {
            float duration = renderImage(false, renderOp, width, height, h_outImage, h_grid);
            //std::cout << "Duration(OpenVDB-Host) = " << duration << " ms" << std::endl;
            durationAvg += duration;
        }
        durationAvg /= numIterations;
        std::cout << "Average Duration(OpenVDB-Host) = " << durationAvg << " ms" << std::endl;

        saveImage("raytrace_fog_volume-openvdb-host.pfm", width, height, (float*)imageBuffer.data());
    }
}

#endif
