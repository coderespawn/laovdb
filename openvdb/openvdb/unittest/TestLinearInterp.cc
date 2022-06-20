// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

#include "gtest/gtest.h"
#include <openvdb/Exceptions.h>
#include <openvdb/openvdb.h>
#include <openvdb/tools/Interpolation.h>
#include <openvdb/math/Stencils.h>

namespace {
// Absolute tolerance for floating-point equality comparisons
const double TOLERANCE = 1.e-6;
}

class TestLinearInterp: public ::testing::Test
{
public:
    template<typename GridType>
    void test();
    template<typename GridType>
    void testTree();
    template<typename GridType>
    void testAccessor();
    template<typename GridType>
    void testConstantValues();
    template<typename GridType>
    void testFillValues();
    template<typename GridType>
    void testNegativeIndices();
    template<typename GridType>
    void testStencilsMatch();
};


template<typename GridType>
void
TestLinearInterp::test()
{
    typename GridType::TreeType TreeType;
    float fillValue = 256.0f;

    GridType grid(fillValue);
    typename GridType::TreeType& tree = grid.tree();

    tree.setValue(laovdb::Coord(10, 10, 10), 1.0);

    tree.setValue(laovdb::Coord(11, 10, 10), 2.0);
    tree.setValue(laovdb::Coord(11, 11, 10), 2.0);
    tree.setValue(laovdb::Coord(10, 11, 10), 2.0);
    tree.setValue(laovdb::Coord( 9, 11, 10), 2.0);
    tree.setValue(laovdb::Coord( 9, 10, 10), 2.0);
    tree.setValue(laovdb::Coord( 9,  9, 10), 2.0);
    tree.setValue(laovdb::Coord(10,  9, 10), 2.0);
    tree.setValue(laovdb::Coord(11,  9, 10), 2.0);

    tree.setValue(laovdb::Coord(10, 10, 11), 3.0);
    tree.setValue(laovdb::Coord(11, 10, 11), 3.0);
    tree.setValue(laovdb::Coord(11, 11, 11), 3.0);
    tree.setValue(laovdb::Coord(10, 11, 11), 3.0);
    tree.setValue(laovdb::Coord( 9, 11, 11), 3.0);
    tree.setValue(laovdb::Coord( 9, 10, 11), 3.0);
    tree.setValue(laovdb::Coord( 9,  9, 11), 3.0);
    tree.setValue(laovdb::Coord(10,  9, 11), 3.0);
    tree.setValue(laovdb::Coord(11,  9, 11), 3.0);

    tree.setValue(laovdb::Coord(10, 10, 9), 4.0);
    tree.setValue(laovdb::Coord(11, 10, 9), 4.0);
    tree.setValue(laovdb::Coord(11, 11, 9), 4.0);
    tree.setValue(laovdb::Coord(10, 11, 9), 4.0);
    tree.setValue(laovdb::Coord( 9, 11, 9), 4.0);
    tree.setValue(laovdb::Coord( 9, 10, 9), 4.0);
    tree.setValue(laovdb::Coord( 9,  9, 9), 4.0);
    tree.setValue(laovdb::Coord(10,  9, 9), 4.0);
    tree.setValue(laovdb::Coord(11,  9, 9), 4.0);

    {//using BoxSampler

        // transform used for worldspace interpolation)
        laovdb::tools::GridSampler<GridType, laovdb::tools::BoxSampler>
            interpolator(grid);
        //laovdb::tools::LinearInterp<GridType> interpolator(*tree);

        typename GridType::ValueType val =
            interpolator.sampleVoxel(10.5, 10.5, 10.5);
        EXPECT_NEAR(2.375, val, TOLERANCE);

        val = interpolator.sampleVoxel(10.0, 10.0, 10.0);
        EXPECT_NEAR(1.0, val, TOLERANCE);

        val = interpolator.sampleVoxel(11.0, 10.0, 10.0);
        EXPECT_NEAR(2.0, val, TOLERANCE);

        val = interpolator.sampleVoxel(11.0, 11.0, 10.0);
        EXPECT_NEAR(2.0, val, TOLERANCE);

        val = interpolator.sampleVoxel(11.0, 11.0, 11.0);
        EXPECT_NEAR(3.0, val, TOLERANCE);

        val = interpolator.sampleVoxel(9.0, 11.0, 9.0);
        EXPECT_NEAR(4.0, val, TOLERANCE);

        val = interpolator.sampleVoxel(9.0, 10.0, 9.0);
        EXPECT_NEAR(4.0, val, TOLERANCE);

        val = interpolator.sampleVoxel(10.1, 10.0, 10.0);
        EXPECT_NEAR(1.1, val, TOLERANCE);

        val = interpolator.sampleVoxel(10.8, 10.8, 10.8);
        EXPECT_NEAR(2.792, val, TOLERANCE);

        val = interpolator.sampleVoxel(10.1, 10.8, 10.5);
        EXPECT_NEAR(2.41, val, TOLERANCE);

        val = interpolator.sampleVoxel(10.8, 10.1, 10.5);
        EXPECT_NEAR(2.41, val, TOLERANCE);

        val = interpolator.sampleVoxel(10.5, 10.1, 10.8);
        EXPECT_NEAR(2.71, val, TOLERANCE);

        val = interpolator.sampleVoxel(10.5, 10.8, 10.1);
        EXPECT_NEAR(2.01, val, TOLERANCE);

    }
    {//using Sampler<1>

        // transform used for worldspace interpolation)
        laovdb::tools::GridSampler<GridType, laovdb::tools::Sampler<1> >
            interpolator(grid);
        //laovdb::tools::LinearInterp<GridType> interpolator(*tree);

        typename GridType::ValueType val =
            interpolator.sampleVoxel(10.5, 10.5, 10.5);
        EXPECT_NEAR(2.375, val, TOLERANCE);

        val = interpolator.sampleVoxel(10.0, 10.0, 10.0);
        EXPECT_NEAR(1.0, val, TOLERANCE);

        val = interpolator.sampleVoxel(11.0, 10.0, 10.0);
        EXPECT_NEAR(2.0, val, TOLERANCE);

        val = interpolator.sampleVoxel(11.0, 11.0, 10.0);
        EXPECT_NEAR(2.0, val, TOLERANCE);

        val = interpolator.sampleVoxel(11.0, 11.0, 11.0);
        EXPECT_NEAR(3.0, val, TOLERANCE);

        val = interpolator.sampleVoxel(9.0, 11.0, 9.0);
        EXPECT_NEAR(4.0, val, TOLERANCE);

        val = interpolator.sampleVoxel(9.0, 10.0, 9.0);
        EXPECT_NEAR(4.0, val, TOLERANCE);

        val = interpolator.sampleVoxel(10.1, 10.0, 10.0);
        EXPECT_NEAR(1.1, val, TOLERANCE);

        val = interpolator.sampleVoxel(10.8, 10.8, 10.8);
        EXPECT_NEAR(2.792, val, TOLERANCE);

        val = interpolator.sampleVoxel(10.1, 10.8, 10.5);
        EXPECT_NEAR(2.41, val, TOLERANCE);

        val = interpolator.sampleVoxel(10.8, 10.1, 10.5);
        EXPECT_NEAR(2.41, val, TOLERANCE);

        val = interpolator.sampleVoxel(10.5, 10.1, 10.8);
        EXPECT_NEAR(2.71, val, TOLERANCE);

        val = interpolator.sampleVoxel(10.5, 10.8, 10.1);
        EXPECT_NEAR(2.01, val, TOLERANCE);
    }
}
TEST_F(TestLinearInterp, testFloat) { test<laovdb::FloatGrid>(); }
TEST_F(TestLinearInterp, testDouble) { test<laovdb::DoubleGrid>(); }

TEST_F(TestLinearInterp, testVec3S)
{
    using namespace laovdb;

    Vec3s fillValue = Vec3s(256.0f, 256.0f, 256.0f);

    Vec3SGrid grid(fillValue);
    Vec3STree& tree = grid.tree();

    tree.setValue(laovdb::Coord(10, 10, 10), Vec3s(1.0, 1.0, 1.0));

    tree.setValue(laovdb::Coord(11, 10, 10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(11, 11, 10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(10, 11, 10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord( 9, 11, 10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord( 9, 10, 10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord( 9,  9, 10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(10,  9, 10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(11,  9, 10), Vec3s(2.0, 2.0, 2.0));

    tree.setValue(laovdb::Coord(10, 10, 11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord(11, 10, 11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord(11, 11, 11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord(10, 11, 11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord( 9, 11, 11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord( 9, 10, 11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord( 9,  9, 11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord(10,  9, 11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord(11,  9, 11), Vec3s(3.0, 3.0, 3.0));

    tree.setValue(laovdb::Coord(10, 10, 9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord(11, 10, 9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord(11, 11, 9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord(10, 11, 9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord( 9, 11, 9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord( 9, 10, 9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord( 9,  9, 9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord(10,  9, 9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord(11,  9, 9), Vec3s(4.0, 4.0, 4.0));

    laovdb::tools::GridSampler<Vec3SGrid, laovdb::tools::BoxSampler>
        interpolator(grid);

    //laovdb::tools::LinearInterp<Vec3STree> interpolator(*tree);

    Vec3SGrid::ValueType val = interpolator.sampleVoxel(10.5, 10.5, 10.5);
    EXPECT_TRUE(val.eq(Vec3s(2.375f)));

    val = interpolator.sampleVoxel(10.0, 10.0, 10.0);
    EXPECT_TRUE(val.eq(Vec3s(1.f)));

    val = interpolator.sampleVoxel(11.0, 10.0, 10.0);
    EXPECT_TRUE(val.eq(Vec3s(2.f)));

    val = interpolator.sampleVoxel(11.0, 11.0, 10.0);
    EXPECT_TRUE(val.eq(Vec3s(2.f)));

    val = interpolator.sampleVoxel(11.0, 11.0, 11.0);
    EXPECT_TRUE(val.eq(Vec3s(3.f)));

    val = interpolator.sampleVoxel(9.0, 11.0, 9.0);
    EXPECT_TRUE(val.eq(Vec3s(4.f)));

    val = interpolator.sampleVoxel(9.0, 10.0, 9.0);
    EXPECT_TRUE(val.eq(Vec3s(4.f)));

    val = interpolator.sampleVoxel(10.1, 10.0, 10.0);
    EXPECT_TRUE(val.eq(Vec3s(1.1f)));

    val = interpolator.sampleVoxel(10.8, 10.8, 10.8);
    EXPECT_TRUE(val.eq(Vec3s(2.792f)));

    val = interpolator.sampleVoxel(10.1, 10.8, 10.5);
    EXPECT_TRUE(val.eq(Vec3s(2.41f)));

    val = interpolator.sampleVoxel(10.8, 10.1, 10.5);
    EXPECT_TRUE(val.eq(Vec3s(2.41f)));

    val = interpolator.sampleVoxel(10.5, 10.1, 10.8);
    EXPECT_TRUE(val.eq(Vec3s(2.71f)));

    val = interpolator.sampleVoxel(10.5, 10.8, 10.1);
    EXPECT_TRUE(val.eq(Vec3s(2.01f)));
}

template<typename GridType>
void
TestLinearInterp::testTree()
{
    float fillValue = 256.0f;
    typedef typename GridType::TreeType TreeType;
    TreeType tree(fillValue);

    tree.setValue(laovdb::Coord(10, 10, 10), 1.0);

    tree.setValue(laovdb::Coord(11, 10, 10), 2.0);
    tree.setValue(laovdb::Coord(11, 11, 10), 2.0);
    tree.setValue(laovdb::Coord(10, 11, 10), 2.0);
    tree.setValue(laovdb::Coord( 9, 11, 10), 2.0);
    tree.setValue(laovdb::Coord( 9, 10, 10), 2.0);
    tree.setValue(laovdb::Coord( 9,  9, 10), 2.0);
    tree.setValue(laovdb::Coord(10,  9, 10), 2.0);
    tree.setValue(laovdb::Coord(11,  9, 10), 2.0);

    tree.setValue(laovdb::Coord(10, 10, 11), 3.0);
    tree.setValue(laovdb::Coord(11, 10, 11), 3.0);
    tree.setValue(laovdb::Coord(11, 11, 11), 3.0);
    tree.setValue(laovdb::Coord(10, 11, 11), 3.0);
    tree.setValue(laovdb::Coord( 9, 11, 11), 3.0);
    tree.setValue(laovdb::Coord( 9, 10, 11), 3.0);
    tree.setValue(laovdb::Coord( 9,  9, 11), 3.0);
    tree.setValue(laovdb::Coord(10,  9, 11), 3.0);
    tree.setValue(laovdb::Coord(11,  9, 11), 3.0);

    tree.setValue(laovdb::Coord(10, 10, 9), 4.0);
    tree.setValue(laovdb::Coord(11, 10, 9), 4.0);
    tree.setValue(laovdb::Coord(11, 11, 9), 4.0);
    tree.setValue(laovdb::Coord(10, 11, 9), 4.0);
    tree.setValue(laovdb::Coord( 9, 11, 9), 4.0);
    tree.setValue(laovdb::Coord( 9, 10, 9), 4.0);
    tree.setValue(laovdb::Coord( 9,  9, 9), 4.0);
    tree.setValue(laovdb::Coord(10,  9, 9), 4.0);
    tree.setValue(laovdb::Coord(11,  9, 9), 4.0);

    // transform used for worldspace interpolation)
    laovdb::tools::GridSampler<TreeType, laovdb::tools::BoxSampler>
        interpolator(tree, laovdb::math::Transform());

    typename GridType::ValueType val =
        interpolator.sampleVoxel(10.5, 10.5, 10.5);
    EXPECT_NEAR(2.375, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.0, 10.0, 10.0);
    EXPECT_NEAR(1.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(11.0, 10.0, 10.0);
    EXPECT_NEAR(2.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(11.0, 11.0, 10.0);
    EXPECT_NEAR(2.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(11.0, 11.0, 11.0);
    EXPECT_NEAR(3.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(9.0, 11.0, 9.0);
    EXPECT_NEAR(4.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(9.0, 10.0, 9.0);
    EXPECT_NEAR(4.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.1, 10.0, 10.0);
    EXPECT_NEAR(1.1, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.8, 10.8, 10.8);
    EXPECT_NEAR(2.792, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.1, 10.8, 10.5);
    EXPECT_NEAR(2.41, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.8, 10.1, 10.5);
    EXPECT_NEAR(2.41, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.5, 10.1, 10.8);
    EXPECT_NEAR(2.71, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.5, 10.8, 10.1);
    EXPECT_NEAR(2.01, val, TOLERANCE);
}
TEST_F(TestLinearInterp, testTreeFloat) { testTree<laovdb::FloatGrid>(); }
TEST_F(TestLinearInterp, testTreeDouble) { testTree<laovdb::DoubleGrid>(); }

TEST_F(TestLinearInterp, testTreeVec3S)
{
    using namespace laovdb;

    Vec3s fillValue = Vec3s(256.0f, 256.0f, 256.0f);

    Vec3STree tree(fillValue);

    tree.setValue(laovdb::Coord(10, 10, 10), Vec3s(1.0, 1.0, 1.0));

    tree.setValue(laovdb::Coord(11, 10, 10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(11, 11, 10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(10, 11, 10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord( 9, 11, 10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord( 9, 10, 10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord( 9,  9, 10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(10,  9, 10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(11,  9, 10), Vec3s(2.0, 2.0, 2.0));

    tree.setValue(laovdb::Coord(10, 10, 11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord(11, 10, 11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord(11, 11, 11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord(10, 11, 11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord( 9, 11, 11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord( 9, 10, 11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord( 9,  9, 11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord(10,  9, 11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord(11,  9, 11), Vec3s(3.0, 3.0, 3.0));

    tree.setValue(laovdb::Coord(10, 10, 9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord(11, 10, 9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord(11, 11, 9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord(10, 11, 9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord( 9, 11, 9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord( 9, 10, 9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord( 9,  9, 9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord(10,  9, 9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord(11,  9, 9), Vec3s(4.0, 4.0, 4.0));

    laovdb::tools::GridSampler<Vec3STree, laovdb::tools::BoxSampler>
        interpolator(tree, laovdb::math::Transform());

    //laovdb::tools::LinearInterp<Vec3STree> interpolator(*tree);

    Vec3SGrid::ValueType val = interpolator.sampleVoxel(10.5, 10.5, 10.5);
    EXPECT_TRUE(val.eq(Vec3s(2.375f)));

    val = interpolator.sampleVoxel(10.0, 10.0, 10.0);
    EXPECT_TRUE(val.eq(Vec3s(1.f)));

    val = interpolator.sampleVoxel(11.0, 10.0, 10.0);
    EXPECT_TRUE(val.eq(Vec3s(2.f)));

    val = interpolator.sampleVoxel(11.0, 11.0, 10.0);
    EXPECT_TRUE(val.eq(Vec3s(2.f)));

    val = interpolator.sampleVoxel(11.0, 11.0, 11.0);
    EXPECT_TRUE(val.eq(Vec3s(3.f)));

    val = interpolator.sampleVoxel(9.0, 11.0, 9.0);
    EXPECT_TRUE(val.eq(Vec3s(4.f)));

    val = interpolator.sampleVoxel(9.0, 10.0, 9.0);
    EXPECT_TRUE(val.eq(Vec3s(4.f)));

    val = interpolator.sampleVoxel(10.1, 10.0, 10.0);
    EXPECT_TRUE(val.eq(Vec3s(1.1f)));

    val = interpolator.sampleVoxel(10.8, 10.8, 10.8);
    EXPECT_TRUE(val.eq(Vec3s(2.792f)));

    val = interpolator.sampleVoxel(10.1, 10.8, 10.5);
    EXPECT_TRUE(val.eq(Vec3s(2.41f)));

    val = interpolator.sampleVoxel(10.8, 10.1, 10.5);
    EXPECT_TRUE(val.eq(Vec3s(2.41f)));

    val = interpolator.sampleVoxel(10.5, 10.1, 10.8);
    EXPECT_TRUE(val.eq(Vec3s(2.71f)));

    val = interpolator.sampleVoxel(10.5, 10.8, 10.1);
    EXPECT_TRUE(val.eq(Vec3s(2.01f)));
}

template<typename GridType>
void
TestLinearInterp::testAccessor()
{
    float fillValue = 256.0f;

    GridType grid(fillValue);
    typedef typename GridType::Accessor AccessorType;

    AccessorType acc = grid.getAccessor();

    acc.setValue(laovdb::Coord(10, 10, 10), 1.0);

    acc.setValue(laovdb::Coord(11, 10, 10), 2.0);
    acc.setValue(laovdb::Coord(11, 11, 10), 2.0);
    acc.setValue(laovdb::Coord(10, 11, 10), 2.0);
    acc.setValue(laovdb::Coord( 9, 11, 10), 2.0);
    acc.setValue(laovdb::Coord( 9, 10, 10), 2.0);
    acc.setValue(laovdb::Coord( 9,  9, 10), 2.0);
    acc.setValue(laovdb::Coord(10,  9, 10), 2.0);
    acc.setValue(laovdb::Coord(11,  9, 10), 2.0);

    acc.setValue(laovdb::Coord(10, 10, 11), 3.0);
    acc.setValue(laovdb::Coord(11, 10, 11), 3.0);
    acc.setValue(laovdb::Coord(11, 11, 11), 3.0);
    acc.setValue(laovdb::Coord(10, 11, 11), 3.0);
    acc.setValue(laovdb::Coord( 9, 11, 11), 3.0);
    acc.setValue(laovdb::Coord( 9, 10, 11), 3.0);
    acc.setValue(laovdb::Coord( 9,  9, 11), 3.0);
    acc.setValue(laovdb::Coord(10,  9, 11), 3.0);
    acc.setValue(laovdb::Coord(11,  9, 11), 3.0);

    acc.setValue(laovdb::Coord(10, 10, 9), 4.0);
    acc.setValue(laovdb::Coord(11, 10, 9), 4.0);
    acc.setValue(laovdb::Coord(11, 11, 9), 4.0);
    acc.setValue(laovdb::Coord(10, 11, 9), 4.0);
    acc.setValue(laovdb::Coord( 9, 11, 9), 4.0);
    acc.setValue(laovdb::Coord( 9, 10, 9), 4.0);
    acc.setValue(laovdb::Coord( 9,  9, 9), 4.0);
    acc.setValue(laovdb::Coord(10,  9, 9), 4.0);
    acc.setValue(laovdb::Coord(11,  9, 9), 4.0);

    // transform used for worldspace interpolation)
    laovdb::tools::GridSampler<AccessorType, laovdb::tools::BoxSampler>
        interpolator(acc, grid.transform());

    typename GridType::ValueType val =
        interpolator.sampleVoxel(10.5, 10.5, 10.5);
    EXPECT_NEAR(2.375, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.0, 10.0, 10.0);
    EXPECT_NEAR(1.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(11.0, 10.0, 10.0);
    EXPECT_NEAR(2.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(11.0, 11.0, 10.0);
    EXPECT_NEAR(2.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(11.0, 11.0, 11.0);
    EXPECT_NEAR(3.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(9.0, 11.0, 9.0);
    EXPECT_NEAR(4.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(9.0, 10.0, 9.0);
    EXPECT_NEAR(4.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.1, 10.0, 10.0);
    EXPECT_NEAR(1.1, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.8, 10.8, 10.8);
    EXPECT_NEAR(2.792, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.1, 10.8, 10.5);
    EXPECT_NEAR(2.41, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.8, 10.1, 10.5);
    EXPECT_NEAR(2.41, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.5, 10.1, 10.8);
    EXPECT_NEAR(2.71, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.5, 10.8, 10.1);
    EXPECT_NEAR(2.01, val, TOLERANCE);
}
TEST_F(TestLinearInterp, testAccessorFloat) { testAccessor<laovdb::FloatGrid>(); }
TEST_F(TestLinearInterp, testAccessorDouble) { testAccessor<laovdb::DoubleGrid>(); }

TEST_F(TestLinearInterp, testAccessorVec3S)
{
    using namespace laovdb;

    Vec3s fillValue = Vec3s(256.0f, 256.0f, 256.0f);

    Vec3SGrid grid(fillValue);
    typedef Vec3SGrid::Accessor AccessorType;
    AccessorType acc = grid.getAccessor();

    acc.setValue(laovdb::Coord(10, 10, 10), Vec3s(1.0, 1.0, 1.0));

    acc.setValue(laovdb::Coord(11, 10, 10), Vec3s(2.0, 2.0, 2.0));
    acc.setValue(laovdb::Coord(11, 11, 10), Vec3s(2.0, 2.0, 2.0));
    acc.setValue(laovdb::Coord(10, 11, 10), Vec3s(2.0, 2.0, 2.0));
    acc.setValue(laovdb::Coord( 9, 11, 10), Vec3s(2.0, 2.0, 2.0));
    acc.setValue(laovdb::Coord( 9, 10, 10), Vec3s(2.0, 2.0, 2.0));
    acc.setValue(laovdb::Coord( 9,  9, 10), Vec3s(2.0, 2.0, 2.0));
    acc.setValue(laovdb::Coord(10,  9, 10), Vec3s(2.0, 2.0, 2.0));
    acc.setValue(laovdb::Coord(11,  9, 10), Vec3s(2.0, 2.0, 2.0));

    acc.setValue(laovdb::Coord(10, 10, 11), Vec3s(3.0, 3.0, 3.0));
    acc.setValue(laovdb::Coord(11, 10, 11), Vec3s(3.0, 3.0, 3.0));
    acc.setValue(laovdb::Coord(11, 11, 11), Vec3s(3.0, 3.0, 3.0));
    acc.setValue(laovdb::Coord(10, 11, 11), Vec3s(3.0, 3.0, 3.0));
    acc.setValue(laovdb::Coord( 9, 11, 11), Vec3s(3.0, 3.0, 3.0));
    acc.setValue(laovdb::Coord( 9, 10, 11), Vec3s(3.0, 3.0, 3.0));
    acc.setValue(laovdb::Coord( 9,  9, 11), Vec3s(3.0, 3.0, 3.0));
    acc.setValue(laovdb::Coord(10,  9, 11), Vec3s(3.0, 3.0, 3.0));
    acc.setValue(laovdb::Coord(11,  9, 11), Vec3s(3.0, 3.0, 3.0));

    acc.setValue(laovdb::Coord(10, 10, 9), Vec3s(4.0, 4.0, 4.0));
    acc.setValue(laovdb::Coord(11, 10, 9), Vec3s(4.0, 4.0, 4.0));
    acc.setValue(laovdb::Coord(11, 11, 9), Vec3s(4.0, 4.0, 4.0));
    acc.setValue(laovdb::Coord(10, 11, 9), Vec3s(4.0, 4.0, 4.0));
    acc.setValue(laovdb::Coord( 9, 11, 9), Vec3s(4.0, 4.0, 4.0));
    acc.setValue(laovdb::Coord( 9, 10, 9), Vec3s(4.0, 4.0, 4.0));
    acc.setValue(laovdb::Coord( 9,  9, 9), Vec3s(4.0, 4.0, 4.0));
    acc.setValue(laovdb::Coord(10,  9, 9), Vec3s(4.0, 4.0, 4.0));
    acc.setValue(laovdb::Coord(11,  9, 9), Vec3s(4.0, 4.0, 4.0));

    laovdb::tools::GridSampler<AccessorType, laovdb::tools::BoxSampler>
        interpolator(acc, grid.transform());

    Vec3SGrid::ValueType val = interpolator.sampleVoxel(10.5, 10.5, 10.5);
    EXPECT_TRUE(val.eq(Vec3s(2.375f)));

    val = interpolator.sampleVoxel(10.0, 10.0, 10.0);
    EXPECT_TRUE(val.eq(Vec3s(1.0f)));

    val = interpolator.sampleVoxel(11.0, 10.0, 10.0);
    EXPECT_TRUE(val.eq(Vec3s(2.0f)));

    val = interpolator.sampleVoxel(11.0, 11.0, 10.0);
    EXPECT_TRUE(val.eq(Vec3s(2.0f)));

    val = interpolator.sampleVoxel(11.0, 11.0, 11.0);
    EXPECT_TRUE(val.eq(Vec3s(3.0f)));

    val = interpolator.sampleVoxel(9.0, 11.0, 9.0);
    EXPECT_TRUE(val.eq(Vec3s(4.0f)));

    val = interpolator.sampleVoxel(9.0, 10.0, 9.0);
    EXPECT_TRUE(val.eq(Vec3s(4.0f)));

    val = interpolator.sampleVoxel(10.1, 10.0, 10.0);
    EXPECT_TRUE(val.eq(Vec3s(1.1f)));

    val = interpolator.sampleVoxel(10.8, 10.8, 10.8);
    EXPECT_TRUE(val.eq(Vec3s(2.792f)));

    val = interpolator.sampleVoxel(10.1, 10.8, 10.5);
    EXPECT_TRUE(val.eq(Vec3s(2.41f)));

    val = interpolator.sampleVoxel(10.8, 10.1, 10.5);
    EXPECT_TRUE(val.eq(Vec3s(2.41f)));

    val = interpolator.sampleVoxel(10.5, 10.1, 10.8);
    EXPECT_TRUE(val.eq(Vec3s(2.71f)));

    val = interpolator.sampleVoxel(10.5, 10.8, 10.1);
    EXPECT_TRUE(val.eq(Vec3s(2.01f)));
}

template<typename GridType>
void
TestLinearInterp::testConstantValues()
{
    typedef typename GridType::TreeType TreeType;
    float fillValue = 256.0f;

    GridType grid(fillValue);
    TreeType& tree = grid.tree();

    // Add values to buffer zero.
    tree.setValue(laovdb::Coord(10, 10, 10), 2.0);

    tree.setValue(laovdb::Coord(11, 10, 10), 2.0);
    tree.setValue(laovdb::Coord(11, 11, 10), 2.0);
    tree.setValue(laovdb::Coord(10, 11, 10), 2.0);
    tree.setValue(laovdb::Coord( 9, 11, 10), 2.0);
    tree.setValue(laovdb::Coord( 9, 10, 10), 2.0);
    tree.setValue(laovdb::Coord( 9,  9, 10), 2.0);
    tree.setValue(laovdb::Coord(10,  9, 10), 2.0);
    tree.setValue(laovdb::Coord(11,  9, 10), 2.0);

    tree.setValue(laovdb::Coord(10, 10, 11), 2.0);
    tree.setValue(laovdb::Coord(11, 10, 11), 2.0);
    tree.setValue(laovdb::Coord(11, 11, 11), 2.0);
    tree.setValue(laovdb::Coord(10, 11, 11), 2.0);
    tree.setValue(laovdb::Coord( 9, 11, 11), 2.0);
    tree.setValue(laovdb::Coord( 9, 10, 11), 2.0);
    tree.setValue(laovdb::Coord( 9,  9, 11), 2.0);
    tree.setValue(laovdb::Coord(10,  9, 11), 2.0);
    tree.setValue(laovdb::Coord(11,  9, 11), 2.0);

    tree.setValue(laovdb::Coord(10, 10, 9), 2.0);
    tree.setValue(laovdb::Coord(11, 10, 9), 2.0);
    tree.setValue(laovdb::Coord(11, 11, 9), 2.0);
    tree.setValue(laovdb::Coord(10, 11, 9), 2.0);
    tree.setValue(laovdb::Coord( 9, 11, 9), 2.0);
    tree.setValue(laovdb::Coord( 9, 10, 9), 2.0);
    tree.setValue(laovdb::Coord( 9,  9, 9), 2.0);
    tree.setValue(laovdb::Coord(10,  9, 9), 2.0);
    tree.setValue(laovdb::Coord(11,  9, 9), 2.0);

    laovdb::tools::GridSampler<TreeType, laovdb::tools::BoxSampler>  interpolator(grid);
    //laovdb::tools::LinearInterp<GridType> interpolator(*tree);

    typename GridType::ValueType val =
        interpolator.sampleVoxel(10.5, 10.5, 10.5);
    EXPECT_NEAR(2.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.0, 10.0, 10.0);
    EXPECT_NEAR(2.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.1, 10.0, 10.0);
    EXPECT_NEAR(2.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.8, 10.8, 10.8);
    EXPECT_NEAR(2.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.1, 10.8, 10.5);
    EXPECT_NEAR(2.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.8, 10.1, 10.5);
    EXPECT_NEAR(2.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.5, 10.1, 10.8);
    EXPECT_NEAR(2.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.5, 10.8, 10.1);
    EXPECT_NEAR(2.0, val, TOLERANCE);
}
TEST_F(TestLinearInterp, testConstantValuesFloat) { testConstantValues<laovdb::FloatGrid>(); }
TEST_F(TestLinearInterp, testConstantValuesDouble) { testConstantValues<laovdb::DoubleGrid>(); }

TEST_F(TestLinearInterp, testConstantValuesVec3S)
{
    using namespace laovdb;

    Vec3s fillValue = Vec3s(256.0f, 256.0f, 256.0f);

    Vec3SGrid grid(fillValue);
    Vec3STree& tree = grid.tree();

    // Add values to buffer zero.
    tree.setValue(laovdb::Coord(10, 10, 10), Vec3s(2.0, 2.0, 2.0));

    tree.setValue(laovdb::Coord(11, 10, 10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(11, 11, 10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(10, 11, 10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord( 9, 11, 10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord( 9, 10, 10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord( 9,  9, 10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(10,  9, 10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(11,  9, 10), Vec3s(2.0, 2.0, 2.0));

    tree.setValue(laovdb::Coord(10, 10, 11), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(11, 10, 11), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(11, 11, 11), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(10, 11, 11), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord( 9, 11, 11), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord( 9, 10, 11), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord( 9,  9, 11), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(10,  9, 11), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(11,  9, 11), Vec3s(2.0, 2.0, 2.0));

    tree.setValue(laovdb::Coord(10, 10, 9), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(11, 10, 9), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(11, 11, 9), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(10, 11, 9), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord( 9, 11, 9), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord( 9, 10, 9), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord( 9,  9, 9), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(10,  9, 9), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(11,  9, 9), Vec3s(2.0, 2.0, 2.0));

    laovdb::tools::GridSampler<Vec3STree, laovdb::tools::BoxSampler>  interpolator(grid);
    //laovdb::tools::LinearInterp<Vec3STree> interpolator(*tree);

    Vec3SGrid::ValueType val = interpolator.sampleVoxel(10.5, 10.5, 10.5);
    EXPECT_TRUE(val.eq(Vec3s(2.0, 2.0, 2.0)));

    val = interpolator.sampleVoxel(10.0, 10.0, 10.0);
    EXPECT_TRUE(val.eq(Vec3s(2.0, 2.0, 2.0)));

    val = interpolator.sampleVoxel(10.1, 10.0, 10.0);
    EXPECT_TRUE(val.eq(Vec3s(2.0, 2.0, 2.0)));

    val = interpolator.sampleVoxel(10.8, 10.8, 10.8);
    EXPECT_TRUE(val.eq(Vec3s(2.0, 2.0, 2.0)));

    val = interpolator.sampleVoxel(10.1, 10.8, 10.5);
    EXPECT_TRUE(val.eq(Vec3s(2.0, 2.0, 2.0)));

    val = interpolator.sampleVoxel(10.8, 10.1, 10.5);
    EXPECT_TRUE(val.eq(Vec3s(2.0, 2.0, 2.0)));

    val = interpolator.sampleVoxel(10.5, 10.1, 10.8);
    EXPECT_TRUE(val.eq(Vec3s(2.0, 2.0, 2.0)));

    val = interpolator.sampleVoxel(10.5, 10.8, 10.1);
    EXPECT_TRUE(val.eq(Vec3s(2.0, 2.0, 2.0)));
}


template<typename GridType>
void
TestLinearInterp::testFillValues()
{
    //typedef typename GridType::TreeType TreeType;
    float fillValue = 256.0f;

    GridType grid(fillValue);
    //typename GridType::TreeType& tree = grid.tree();

    laovdb::tools::GridSampler<GridType, laovdb::tools::BoxSampler>
        interpolator(grid);
    //laovdb::tools::LinearInterp<GridType> interpolator(*tree);

    typename GridType::ValueType val =
        interpolator.sampleVoxel(10.5, 10.5, 10.5);
    EXPECT_NEAR(256.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.0, 10.0, 10.0);
    EXPECT_NEAR(256.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.1, 10.0, 10.0);
    EXPECT_NEAR(256.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.8, 10.8, 10.8);
    EXPECT_NEAR(256.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.1, 10.8, 10.5);
    EXPECT_NEAR(256.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.8, 10.1, 10.5);
    EXPECT_NEAR(256.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.5, 10.1, 10.8);
    EXPECT_NEAR(256.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(10.5, 10.8, 10.1);
    EXPECT_NEAR(256.0, val, TOLERANCE);
}
TEST_F(TestLinearInterp, testFillValuesFloat) { testFillValues<laovdb::FloatGrid>(); }
TEST_F(TestLinearInterp, testFillValuesDouble) { testFillValues<laovdb::DoubleGrid>(); }

TEST_F(TestLinearInterp, testFillValuesVec3S)
{
    using namespace laovdb;

    Vec3s fillValue = Vec3s(256.0f, 256.0f, 256.0f);

    Vec3SGrid grid(fillValue);
    //Vec3STree& tree = grid.tree();

    laovdb::tools::GridSampler<Vec3SGrid, laovdb::tools::BoxSampler>
        interpolator(grid);
    //laovdb::tools::LinearInterp<Vec3STree> interpolator(*tree);

    Vec3SGrid::ValueType val = interpolator.sampleVoxel(10.5, 10.5, 10.5);
    EXPECT_TRUE(val.eq(Vec3s(256.0, 256.0, 256.0)));

    val = interpolator.sampleVoxel(10.0, 10.0, 10.0);
    EXPECT_TRUE(val.eq(Vec3s(256.0, 256.0, 256.0)));

    val = interpolator.sampleVoxel(10.1, 10.0, 10.0);
    EXPECT_TRUE(val.eq(Vec3s(256.0, 256.0, 256.0)));

    val = interpolator.sampleVoxel(10.8, 10.8, 10.8);
    EXPECT_TRUE(val.eq(Vec3s(256.0, 256.0, 256.0)));

    val = interpolator.sampleVoxel(10.1, 10.8, 10.5);
    EXPECT_TRUE(val.eq(Vec3s(256.0, 256.0, 256.0)));

    val = interpolator.sampleVoxel(10.8, 10.1, 10.5);
    EXPECT_TRUE(val.eq(Vec3s(256.0, 256.0, 256.0)));

    val = interpolator.sampleVoxel(10.5, 10.1, 10.8);
    EXPECT_TRUE(val.eq(Vec3s(256.0, 256.0, 256.0)));

    val = interpolator.sampleVoxel(10.5, 10.8, 10.1);
    EXPECT_TRUE(val.eq(Vec3s(256.0, 256.0, 256.0)));
}


template<typename GridType>
void
TestLinearInterp::testNegativeIndices()
{
    typedef typename GridType::TreeType TreeType;
    float fillValue = 256.0f;

    GridType grid(fillValue);
    TreeType& tree = grid.tree();

    tree.setValue(laovdb::Coord(-10, -10, -10), 1.0);

    tree.setValue(laovdb::Coord(-11, -10, -10), 2.0);
    tree.setValue(laovdb::Coord(-11, -11, -10), 2.0);
    tree.setValue(laovdb::Coord(-10, -11, -10), 2.0);
    tree.setValue(laovdb::Coord( -9, -11, -10), 2.0);
    tree.setValue(laovdb::Coord( -9, -10, -10), 2.0);
    tree.setValue(laovdb::Coord( -9,  -9, -10), 2.0);
    tree.setValue(laovdb::Coord(-10,  -9, -10), 2.0);
    tree.setValue(laovdb::Coord(-11,  -9, -10), 2.0);

    tree.setValue(laovdb::Coord(-10, -10, -11), 3.0);
    tree.setValue(laovdb::Coord(-11, -10, -11), 3.0);
    tree.setValue(laovdb::Coord(-11, -11, -11), 3.0);
    tree.setValue(laovdb::Coord(-10, -11, -11), 3.0);
    tree.setValue(laovdb::Coord( -9, -11, -11), 3.0);
    tree.setValue(laovdb::Coord( -9, -10, -11), 3.0);
    tree.setValue(laovdb::Coord( -9,  -9, -11), 3.0);
    tree.setValue(laovdb::Coord(-10,  -9, -11), 3.0);
    tree.setValue(laovdb::Coord(-11,  -9, -11), 3.0);

    tree.setValue(laovdb::Coord(-10, -10, -9), 4.0);
    tree.setValue(laovdb::Coord(-11, -10, -9), 4.0);
    tree.setValue(laovdb::Coord(-11, -11, -9), 4.0);
    tree.setValue(laovdb::Coord(-10, -11, -9), 4.0);
    tree.setValue(laovdb::Coord( -9, -11, -9), 4.0);
    tree.setValue(laovdb::Coord( -9, -10, -9), 4.0);
    tree.setValue(laovdb::Coord( -9,  -9, -9), 4.0);
    tree.setValue(laovdb::Coord(-10,  -9, -9), 4.0);
    tree.setValue(laovdb::Coord(-11,  -9, -9), 4.0);

    //laovdb::tools::LinearInterp<GridType> interpolator(*tree);
    laovdb::tools::GridSampler<TreeType, laovdb::tools::BoxSampler>  interpolator(grid);

    typename GridType::ValueType val =
        interpolator.sampleVoxel(-10.5, -10.5, -10.5);
    EXPECT_NEAR(2.375, val, TOLERANCE);

    val = interpolator.sampleVoxel(-10.0, -10.0, -10.0);
    EXPECT_NEAR(1.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(-11.0, -10.0, -10.0);
    EXPECT_NEAR(2.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(-11.0, -11.0, -10.0);
    EXPECT_NEAR(2.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(-11.0, -11.0, -11.0);
    EXPECT_NEAR(3.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(-9.0, -11.0, -9.0);
    EXPECT_NEAR(4.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(-9.0, -10.0, -9.0);
    EXPECT_NEAR(4.0, val, TOLERANCE);

    val = interpolator.sampleVoxel(-10.1, -10.0, -10.0);
    EXPECT_NEAR(1.1, val, TOLERANCE);

    val = interpolator.sampleVoxel(-10.8, -10.8, -10.8);
    EXPECT_NEAR(2.792, val, TOLERANCE);

    val = interpolator.sampleVoxel(-10.1, -10.8, -10.5);
    EXPECT_NEAR(2.41, val, TOLERANCE);

    val = interpolator.sampleVoxel(-10.8, -10.1, -10.5);
    EXPECT_NEAR(2.41, val, TOLERANCE);

    val = interpolator.sampleVoxel(-10.5, -10.1, -10.8);
    EXPECT_NEAR(2.71, val, TOLERANCE);

    val = interpolator.sampleVoxel(-10.5, -10.8, -10.1);
    EXPECT_NEAR(2.01, val, TOLERANCE);
}
TEST_F(TestLinearInterp, testNegativeIndicesFloat) { testNegativeIndices<laovdb::FloatGrid>(); }
TEST_F(TestLinearInterp, testNegativeIndicesDouble) { testNegativeIndices<laovdb::DoubleGrid>(); }

TEST_F(TestLinearInterp, testNegativeIndicesVec3S)
{
    using namespace laovdb;

    Vec3s fillValue = Vec3s(256.0f, 256.0f, 256.0f);

    Vec3SGrid grid(fillValue);
    Vec3STree& tree = grid.tree();

    tree.setValue(laovdb::Coord(-10, -10, -10), Vec3s(1.0, 1.0, 1.0));

    tree.setValue(laovdb::Coord(-11, -10, -10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(-11, -11, -10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(-10, -11, -10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord( -9, -11, -10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord( -9, -10, -10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord( -9,  -9, -10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(-10,  -9, -10), Vec3s(2.0, 2.0, 2.0));
    tree.setValue(laovdb::Coord(-11,  -9, -10), Vec3s(2.0, 2.0, 2.0));

    tree.setValue(laovdb::Coord(-10, -10, -11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord(-11, -10, -11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord(-11, -11, -11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord(-10, -11, -11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord( -9, -11, -11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord( -9, -10, -11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord( -9,  -9, -11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord(-10,  -9, -11), Vec3s(3.0, 3.0, 3.0));
    tree.setValue(laovdb::Coord(-11,  -9, -11), Vec3s(3.0, 3.0, 3.0));

    tree.setValue(laovdb::Coord(-10, -10, -9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord(-11, -10, -9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord(-11, -11, -9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord(-10, -11, -9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord( -9, -11, -9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord( -9, -10, -9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord( -9,  -9, -9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord(-10,  -9, -9), Vec3s(4.0, 4.0, 4.0));
    tree.setValue(laovdb::Coord(-11,  -9, -9), Vec3s(4.0, 4.0, 4.0));

    laovdb::tools::GridSampler<Vec3SGrid, laovdb::tools::BoxSampler>  interpolator(grid);
    //laovdb::tools::LinearInterp<Vec3STree> interpolator(*tree);

    Vec3SGrid::ValueType val = interpolator.sampleVoxel(-10.5, -10.5, -10.5);
    EXPECT_TRUE(val.eq(Vec3s(2.375f)));

    val = interpolator.sampleVoxel(-10.0, -10.0, -10.0);
    EXPECT_TRUE(val.eq(Vec3s(1.0f)));

    val = interpolator.sampleVoxel(-11.0, -10.0, -10.0);
    EXPECT_TRUE(val.eq(Vec3s(2.0f)));

    val = interpolator.sampleVoxel(-11.0, -11.0, -10.0);
    EXPECT_TRUE(val.eq(Vec3s(2.0f)));

    val = interpolator.sampleVoxel(-11.0, -11.0, -11.0);
    EXPECT_TRUE(val.eq(Vec3s(3.0f)));

    val = interpolator.sampleVoxel(-9.0, -11.0, -9.0);
    EXPECT_TRUE(val.eq(Vec3s(4.0f)));

    val = interpolator.sampleVoxel(-9.0, -10.0, -9.0);
    EXPECT_TRUE(val.eq(Vec3s(4.0f)));

    val = interpolator.sampleVoxel(-10.1, -10.0, -10.0);
    EXPECT_TRUE(val.eq(Vec3s(1.1f)));

    val = interpolator.sampleVoxel(-10.8, -10.8, -10.8);
    EXPECT_TRUE(val.eq(Vec3s(2.792f)));

    val = interpolator.sampleVoxel(-10.1, -10.8, -10.5);
    EXPECT_TRUE(val.eq(Vec3s(2.41f)));

    val = interpolator.sampleVoxel(-10.8, -10.1, -10.5);
    EXPECT_TRUE(val.eq(Vec3s(2.41f)));

    val = interpolator.sampleVoxel(-10.5, -10.1, -10.8);
    EXPECT_TRUE(val.eq(Vec3s(2.71f)));

    val = interpolator.sampleVoxel(-10.5, -10.8, -10.1);
    EXPECT_TRUE(val.eq(Vec3s(2.01f)));
}


template<typename GridType>
void
TestLinearInterp::testStencilsMatch()
{
    typedef typename GridType::ValueType ValueType;

    GridType grid;
    typename GridType::TreeType& tree = grid.tree();

    // using mostly recurring numbers

    tree.setValue(laovdb::Coord(0, 0, 0), ValueType(1.0/3.0));
    tree.setValue(laovdb::Coord(0, 1, 0), ValueType(1.0/11.0));
    tree.setValue(laovdb::Coord(0, 0, 1), ValueType(1.0/81.0));
    tree.setValue(laovdb::Coord(1, 0, 0), ValueType(1.0/97.0));
    tree.setValue(laovdb::Coord(1, 1, 0), ValueType(1.0/61.0));
    tree.setValue(laovdb::Coord(0, 1, 1), ValueType(9.0/7.0));
    tree.setValue(laovdb::Coord(1, 0, 1), ValueType(9.0/11.0));
    tree.setValue(laovdb::Coord(1, 1, 1), ValueType(22.0/7.0));

    const laovdb::Vec3f pos(7.0f/12.0f, 1.0f/3.0f, 2.0f/3.0f);

    {//using BoxSampler and BoxStencil

        laovdb::tools::GridSampler<GridType, laovdb::tools::BoxSampler>
            interpolator(grid);

        laovdb::math::BoxStencil<const GridType>
            stencil(grid);

        typename GridType::ValueType val1 = interpolator.sampleVoxel(pos.x(), pos.y(), pos.z());

        stencil.moveTo(pos);
        typename GridType::ValueType val2 = stencil.interpolation(pos);
        EXPECT_EQ(val1, val2);
    }
}
TEST_F(TestLinearInterp, testStencilsMatchFloat) { testStencilsMatch<laovdb::FloatGrid>(); }
TEST_F(TestLinearInterp, testStencilsMatchDouble) { testStencilsMatch<laovdb::DoubleGrid>(); }
