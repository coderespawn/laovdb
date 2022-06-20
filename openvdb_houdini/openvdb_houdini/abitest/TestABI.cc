// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

#include <openvdb/openvdb.h>
#include <openvdb/tools/LevelSetSphere.h>
#include <openvdb/points/PointDataGrid.h>
#include <openvdb/points/PointConversion.h>

#include <stdexcept>

#ifdef HOUDINI
namespace houdini {
#endif

////////////////////////////////////////

// Validation Methods

// throw an exception if the condition is false
inline void VDB_ASSERT(const bool condition, const std::string& file, const int line)
{
    if (!condition) {
        throw std::runtime_error("Assertion Fail in file " + file + " on line " + std::to_string(line));
    }
}

#define VDB_ASSERT(condition) VDB_ASSERT(condition, __FILE__, __LINE__)

////////////////////////////////////////

// Version methods

const char* getABI()
{
    return OPENVDB_PREPROC_STRINGIFY(OPENVDB_ABI_VERSION_NUMBER);
}

const char* getNamespace()
{
    return OPENVDB_PREPROC_STRINGIFY(OPENVDB_VERSION_NAME);
}

////////////////////////////////////////

// Grid Methods

void* createFloatGrid()
{
    laovdb::initialize();

    laovdb::FloatGrid::Ptr grid =
        laovdb::tools::createLevelSetSphere<laovdb::FloatGrid>(
            /*radius=*/1.0f, /*center=*/laovdb::Vec3f(0.0f), /*voxelSize=*/0.1f);

    return new laovdb::FloatGrid(*grid);
}

void* createPointsGrid()
{
    laovdb::initialize();

    const std::vector<laovdb::Vec3R> pos {
        laovdb::Vec3R(0,0,0),
        laovdb::Vec3R(10,10,10),
        laovdb::Vec3R(10,-10,10),
        laovdb::Vec3R(10,10,-10),
        laovdb::Vec3R(10,-10,-10),
        laovdb::Vec3R(-10,10,-10),
        laovdb::Vec3R(-10,10,10),
        laovdb::Vec3R(-10,-10,10),
        laovdb::Vec3R(-10,-10,-10)
    };

    auto transform = laovdb::math::Transform::createLinearTransform(0.1);

    laovdb::points::PointDataGrid::Ptr grid =
        laovdb::points::createPointDataGrid<laovdb::points::NullCodec,
            laovdb::points::PointDataGrid, laovdb::Vec3R>(pos, *transform);

    return new laovdb::points::PointDataGrid(*grid);
}

void cleanupFloatGrid(void* gridPtr)
{
    laovdb::uninitialize();

    laovdb::FloatGrid* grid =
        static_cast<laovdb::FloatGrid*>(gridPtr);
    delete grid;
}

void cleanupPointsGrid(void* gridPtr)
{
    laovdb::uninitialize();

    laovdb::points::PointDataGrid* grid =
        static_cast<laovdb::points::PointDataGrid*>(gridPtr);
    delete grid;
}

int validateFloatGrid(void* gridPtr)
{
    laovdb::FloatGrid* grid =
        static_cast<laovdb::FloatGrid*>(gridPtr);

    VDB_ASSERT(grid);
    VDB_ASSERT(grid->tree().activeVoxelCount() > laovdb::Index64(0));
    VDB_ASSERT(grid->tree().leafCount() > laovdb::Index64(0));

    std::stringstream ss;
    grid->tree().print(ss);
    VDB_ASSERT(ss.str().length() > size_t(0));

    auto iter = grid->tree().cbeginLeaf();
    VDB_ASSERT(iter);
    VDB_ASSERT(iter->memUsage() > laovdb::Index64(0));

    return 0;
}

int validatePointsGrid(void* gridPtr)
{
    laovdb::points::PointDataGrid* grid =
        static_cast<laovdb::points::PointDataGrid*>(gridPtr);

    VDB_ASSERT(grid);
    VDB_ASSERT(grid->tree().activeVoxelCount() > laovdb::Index64(0));
    VDB_ASSERT(grid->tree().leafCount() > laovdb::Index64(0));

    std::stringstream ss;
    grid->tree().print(ss);
    VDB_ASSERT(ss.str().length() > size_t(0));

    auto iter = grid->tree().cbeginLeaf();
    VDB_ASSERT(iter);
    VDB_ASSERT(iter->memUsage() > laovdb::Index64(0));

    auto handle = laovdb::points::AttributeHandle<laovdb::Vec3f>::create(iter->constAttributeArray("P"));
    VDB_ASSERT(handle->get(0) == laovdb::Vec3f(0));

    return 0;
}

#ifdef HOUDINI
} // namespace houdini
#endif
