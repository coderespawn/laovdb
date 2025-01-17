// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

#include <openvdb/openvdb.h>
#include <openvdb/math/Math.h> // for math::Random01
#include <openvdb/tools/PointsToMask.h>
#include <openvdb/util/CpuTimer.h>
#include "gtest/gtest.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include "util.h" // for genPoints


struct TestPointsToMask: public ::testing::Test
{
};


////////////////////////////////////////

namespace {

class PointList
{
public:
    PointList(const std::vector<laovdb::Vec3R>& points) : mPoints(&points) {}

    size_t size() const { return mPoints->size(); }

    void getPos(size_t n, laovdb::Vec3R& xyz) const { xyz = (*mPoints)[n]; }
protected:
    std::vector<laovdb::Vec3R> const * const mPoints;
}; // PointList

} // namespace



////////////////////////////////////////


TEST_F(TestPointsToMask, testPointsToMask)
{
    {// BoolGrid
        // generate one point
        std::vector<laovdb::Vec3R> points;
        points.push_back( laovdb::Vec3R(-19.999, 4.50001, 6.71) );
        //points.push_back( laovdb::Vec3R( 20,-4.5,-5.2) );
        PointList pointList(points);

        // construct an empty mask grid
        laovdb::BoolGrid grid( false );
        const float voxelSize = 0.1f;
        grid.setTransform( laovdb::math::Transform::createLinearTransform(voxelSize) );
        EXPECT_TRUE( grid.empty() );

        // generate mask from points
        laovdb::tools::PointsToMask<laovdb::BoolGrid> mask( grid );
        mask.addPoints( pointList );
        EXPECT_TRUE(!grid.empty() );
        EXPECT_EQ( 1, int(grid.activeVoxelCount()) );
        laovdb::BoolGrid::ValueOnCIter iter = grid.cbeginValueOn();
        //std::cerr << "Coord = " << iter.getCoord() << std::endl;
        const laovdb::Coord p(-200, 45, 67);
        EXPECT_TRUE( iter.getCoord() == p );
        EXPECT_TRUE(grid.tree().isValueOn( p ) );
    }

    {// MaskGrid
        // generate one point
        std::vector<laovdb::Vec3R> points;
        points.push_back( laovdb::Vec3R(-19.999, 4.50001, 6.71) );
        //points.push_back( laovdb::Vec3R( 20,-4.5,-5.2) );
        PointList pointList(points);

        // construct an empty mask grid
        laovdb::MaskGrid grid( false );
        const float voxelSize = 0.1f;
        grid.setTransform( laovdb::math::Transform::createLinearTransform(voxelSize) );
        EXPECT_TRUE( grid.empty() );

        // generate mask from points
        laovdb::tools::PointsToMask<> mask( grid );
        mask.addPoints( pointList );
        EXPECT_TRUE(!grid.empty() );
        EXPECT_EQ( 1, int(grid.activeVoxelCount()) );
        laovdb::TopologyGrid::ValueOnCIter iter = grid.cbeginValueOn();
        //std::cerr << "Coord = " << iter.getCoord() << std::endl;
        const laovdb::Coord p(-200, 45, 67);
        EXPECT_TRUE( iter.getCoord() == p );
        EXPECT_TRUE(grid.tree().isValueOn( p ) );
    }


    // generate shared transformation
    laovdb::Index64 voxelCount = 0;
    const float voxelSize = 0.001f;
    const laovdb::math::Transform::Ptr xform =
        laovdb::math::Transform::createLinearTransform(voxelSize);

    // generate lots of points
    std::vector<laovdb::Vec3R> points;
    unittest_util::genPoints(15000000, points);
    PointList pointList(points);

    //laovdb::util::CpuTimer timer;
    {// serial BoolGrid
        // construct an empty mask grid
        laovdb::BoolGrid grid( false );
        grid.setTransform( xform );
        EXPECT_TRUE( grid.empty() );

        // generate mask from points
        laovdb::tools::PointsToMask<laovdb::BoolGrid> mask( grid );
        //timer.start("\nSerial BoolGrid");
        mask.addPoints( pointList, 0 );
        //timer.stop();

        EXPECT_TRUE(!grid.empty() );
        //grid.print(std::cerr, 3);
        voxelCount = grid.activeVoxelCount();
    }
    {// parallel BoolGrid
        // construct an empty mask grid
        laovdb::BoolGrid grid( false );
        grid.setTransform( xform );
        EXPECT_TRUE( grid.empty() );

        // generate mask from points
        laovdb::tools::PointsToMask<laovdb::BoolGrid> mask( grid );
        //timer.start("\nParallel BoolGrid");
        mask.addPoints( pointList );
        //timer.stop();

        EXPECT_TRUE(!grid.empty() );
        //grid.print(std::cerr, 3);
        EXPECT_EQ( voxelCount, grid.activeVoxelCount() );
    }
    {// parallel MaskGrid
        // construct an empty mask grid
        laovdb::MaskGrid grid( false );
        grid.setTransform( xform );
        EXPECT_TRUE( grid.empty() );

        // generate mask from points
        laovdb::tools::PointsToMask<> mask( grid );
        //timer.start("\nParallel MaskGrid");
        mask.addPoints( pointList );
        //timer.stop();

        EXPECT_TRUE(!grid.empty() );
        //grid.print(std::cerr, 3);
        EXPECT_EQ( voxelCount, grid.activeVoxelCount() );
    }
    {// parallel create TopologyGrid
        //timer.start("\nParallel Create MaskGrid");
        laovdb::MaskGrid::Ptr grid = laovdb::tools::createPointMask(pointList, *xform);
        //timer.stop();

        EXPECT_TRUE(!grid->empty() );
        //grid->print(std::cerr, 3);
        EXPECT_EQ( voxelCount, grid->activeVoxelCount() );
    }
}
