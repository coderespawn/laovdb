// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

#include <openvdb/openvdb.h>
#include <openvdb/Exceptions.h>
#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/util/Util.h>

#include <gtest/gtest.h>
#include <vector>

class TestMeshToVolume: public ::testing::Test
{
};


////////////////////////////////////////


TEST_F(TestMeshToVolume, testUtils)
{
    /// Test nearestCoord
    laovdb::Vec3d xyz(0.7, 2.2, -2.7);
    laovdb::Coord ijk = laovdb::util::nearestCoord(xyz);
    EXPECT_TRUE(ijk[0] == 0 && ijk[1] == 2 && ijk[2] == -3);

    xyz = laovdb::Vec3d(-22.1, 4.6, 202.34);
    ijk = laovdb::util::nearestCoord(xyz);
    EXPECT_TRUE(ijk[0] == -23 && ijk[1] == 4 && ijk[2] == 202);

    /// Test the coordinate offset table for neghbouring voxels
    laovdb::Coord sum(0, 0, 0);

    unsigned int pX = 0, pY = 0, pZ = 0, mX = 0, mY = 0, mZ = 0;

    for (unsigned int i = 0; i < 26; ++i) {
        ijk = laovdb::util::COORD_OFFSETS[i];
        sum += ijk;

        if (ijk[0] == 1)       ++pX;
        else if (ijk[0] == -1) ++mX;

        if (ijk[1] == 1)       ++pY;
        else if (ijk[1] == -1) ++mY;

        if (ijk[2] == 1)       ++pZ;
        else if (ijk[2] == -1) ++mZ;
    }

    EXPECT_TRUE(sum == laovdb::Coord(0, 0, 0));

    EXPECT_TRUE( pX == 9);
    EXPECT_TRUE( pY == 9);
    EXPECT_TRUE( pZ == 9);
    EXPECT_TRUE( mX == 9);
    EXPECT_TRUE( mY == 9);
    EXPECT_TRUE( mZ == 9);
}

TEST_F(TestMeshToVolume, testConversion)
{
    using namespace laovdb;

    std::vector<Vec3s> points;
    std::vector<Vec4I> quads;

    // cube vertices
    points.push_back(Vec3s(2, 2, 2)); // 0       6--------7
    points.push_back(Vec3s(5, 2, 2)); // 1      /|       /|
    points.push_back(Vec3s(2, 5, 2)); // 2     2--------3 |
    points.push_back(Vec3s(5, 5, 2)); // 3     | |      | |
    points.push_back(Vec3s(2, 2, 5)); // 4     | 4------|-5
    points.push_back(Vec3s(5, 2, 5)); // 5     |/       |/
    points.push_back(Vec3s(2, 5, 5)); // 6     0--------1
    points.push_back(Vec3s(5, 5, 5)); // 7

    // cube faces
    quads.push_back(Vec4I(0, 1, 3, 2)); // front
    quads.push_back(Vec4I(5, 4, 6, 7)); // back
    quads.push_back(Vec4I(0, 2, 6, 4)); // left
    quads.push_back(Vec4I(1, 5, 7, 3)); // right
    quads.push_back(Vec4I(2, 3, 7, 6)); // top
    quads.push_back(Vec4I(0, 4, 5, 1)); // bottom

    math::Transform::Ptr xform = math::Transform::createLinearTransform();

    tools::QuadAndTriangleDataAdapter<Vec3s, Vec4I> mesh(points, quads);

    FloatGrid::Ptr grid = tools::meshToVolume<FloatGrid>(mesh, *xform);

    EXPECT_TRUE(grid.get() != NULL);
    EXPECT_EQ(int(GRID_LEVEL_SET), int(grid->getGridClass()));
    EXPECT_EQ(1, int(grid->baseTree().leafCount()));

    grid = tools::meshToLevelSet<FloatGrid>(*xform, points, quads);

    EXPECT_TRUE(grid.get() != NULL);
    EXPECT_EQ(int(GRID_LEVEL_SET), int(grid->getGridClass()));
    EXPECT_EQ(1, int(grid->baseTree().leafCount()));
}


TEST_F(TestMeshToVolume, testCreateLevelSetBox)
{
    typedef laovdb::FloatGrid          FloatGrid;
    typedef laovdb::Vec3s              Vec3s;
    typedef laovdb::math::BBox<Vec3s>  BBoxs;
    typedef laovdb::math::Transform    Transform;

    BBoxs bbox(Vec3s(0.0, 0.0, 0.0), Vec3s(1.0, 1.0, 1.0));

    Transform::Ptr transform = Transform::createLinearTransform(0.1);

    FloatGrid::Ptr grid = laovdb::tools::createLevelSetBox<FloatGrid>(bbox, *transform);

    double gridBackground = grid->background();
    double expectedBackground = transform->voxelSize().x() * double(laovdb::LEVEL_SET_HALF_WIDTH);

    EXPECT_NEAR(expectedBackground, gridBackground, 1e-6);

    EXPECT_TRUE(grid->tree().leafCount() > 0);

    // test inside coord value
    laovdb::Coord ijk = transform->worldToIndexNodeCentered(laovdb::Vec3d(0.5, 0.5, 0.5));
    EXPECT_TRUE(grid->tree().getValue(ijk) < 0.0f);

    // test outside coord value
    ijk = transform->worldToIndexNodeCentered(laovdb::Vec3d(1.5, 1.5, 1.5));
    EXPECT_TRUE(grid->tree().getValue(ijk) > 0.0f);
}

