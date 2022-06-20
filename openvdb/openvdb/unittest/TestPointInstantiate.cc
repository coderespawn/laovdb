// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

// this removes the PointDataGrid and PointDataTree aliases
#define OPENVDB_DISABLE_POINT_DATA_TREE_ALIAS

// include all of the point headers and confirm that none are referring directly
// to the removed aliases

#include <openvdb/points/PointAdvect.h>
#include <openvdb/points/PointAttribute.h>
#include <openvdb/points/PointConversion.h>
#include <openvdb/points/PointCount.h>
#include <openvdb/points/PointDataGrid.h>
#include <openvdb/points/PointDelete.h>
#include <openvdb/points/PointGroup.h>
#include <openvdb/points/PointMask.h>
#include <openvdb/points/PointMove.h>
#include <openvdb/points/PointSample.h>
#include <openvdb/points/PointScatter.h>

#include <gtest/gtest.h>


class TestPointInstantiate: public ::testing::Test
{
}; // class TestPointInstantiate


TEST_F(TestPointInstantiate, test)
{
    laovdb::initialize();

    std::vector<laovdb::Vec3f> positions;
    positions.emplace_back(1.0f, 2.0f, 3.0f);
    laovdb::points::PointAttributeVector<laovdb::Vec3f> wrapper(positions);

    auto transform = laovdb::math::Transform::createLinearTransform(0.5);

    // these custom grid types use a 64-bit value type instead of a 32-bit value type
    // and have a 16^3 leaf node instead of a 8^3 leaf node

    using CustomPointIndexGrid = laovdb::Grid<laovdb::tree::Tree<laovdb::tree::RootNode<
        laovdb::tree::InternalNode<laovdb::tree::InternalNode<
        laovdb::tools::PointIndexLeafNode<laovdb::PointIndex64, 4>, 4>, 5>>>>;
    using CustomPointDataGrid = laovdb::Grid<laovdb::tree::Tree<laovdb::tree::RootNode<
        laovdb::tree::InternalNode<laovdb::tree::InternalNode<
        laovdb::points::PointDataLeafNode<laovdb::PointDataIndex64, 4>, 4>, 5>>>>;

    auto pointIndexGrid = laovdb::tools::createPointIndexGrid<CustomPointIndexGrid>(
        wrapper, *transform);
    auto points = laovdb::points::createPointDataGrid<laovdb::points::NullCodec, CustomPointDataGrid>(
        *pointIndexGrid, wrapper, *transform);

    EXPECT_TRUE(points);
}
