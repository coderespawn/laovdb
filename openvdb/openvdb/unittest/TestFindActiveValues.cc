// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

#include <openvdb/Exceptions.h>
#include <openvdb/Types.h>
#include <openvdb/openvdb.h>
#include <openvdb/util/CpuTimer.h>
#include <openvdb/tools/LevelSetSphere.h>
#include <openvdb/tools/FindActiveValues.h>

#include <gtest/gtest.h>

#include "util.h" // for unittest_util::makeSphere()

#include <cstdio> // for remove()
#include <fstream>
#include <sstream>

#define ASSERT_DOUBLES_EXACTLY_EQUAL(expected, actual) \
    EXPECT_NEAR((expected), (actual), /*tolerance=*/0.0);


class TestFindActiveValues: public ::testing::Test
{
public:
    void SetUp() override { laovdb::initialize(); }
    void TearDown() override { laovdb::uninitialize(); }
};


TEST_F(TestFindActiveValues, testBasic)
{
    const float background = 5.0f;
    laovdb::FloatTree tree(background);
    const laovdb::Coord min(-1,-2,30), max(20,30,55);
    const laovdb::CoordBBox bbox(min[0], min[1], min[2],
                                  max[0], max[1], max[2]);

    EXPECT_TRUE( laovdb::tools::noActiveValues(tree, bbox));
    EXPECT_TRUE(!laovdb::tools::anyActiveValues(tree, bbox));
    EXPECT_TRUE(!laovdb::tools::anyActiveVoxels(tree, bbox));

    tree.setValue(min.offsetBy(-1), 1.0f);
    EXPECT_TRUE( laovdb::tools::noActiveValues(tree, bbox));
    EXPECT_TRUE(!laovdb::tools::anyActiveValues(tree, bbox));
    EXPECT_TRUE(!laovdb::tools::anyActiveVoxels(tree, bbox));

    tree.setValue(max.offsetBy( 1), 1.0f);
    EXPECT_TRUE( laovdb::tools::noActiveValues(tree, bbox));
    EXPECT_TRUE(!laovdb::tools::anyActiveValues(tree, bbox));
    EXPECT_TRUE(!laovdb::tools::anyActiveVoxels(tree, bbox));

    tree.setValue(min, 1.0f);
    EXPECT_TRUE(laovdb::tools::anyActiveValues(tree, bbox));
    EXPECT_TRUE(laovdb::tools::anyActiveVoxels(tree, bbox));
    EXPECT_TRUE(!laovdb::tools::anyActiveTiles(tree, bbox));

    tree.setValue(max, 1.0f);
    EXPECT_TRUE(laovdb::tools::anyActiveValues(tree, bbox));
    EXPECT_TRUE(laovdb::tools::anyActiveVoxels(tree, bbox));
    EXPECT_TRUE(!laovdb::tools::anyActiveTiles(tree, bbox));
    auto tiles = laovdb::tools::activeTiles(tree, bbox);
    EXPECT_TRUE( tiles.size() == 0u );

    tree.sparseFill(bbox, 1.0f);
    EXPECT_TRUE(laovdb::tools::anyActiveValues(tree, bbox));
    EXPECT_TRUE(laovdb::tools::anyActiveVoxels(tree, bbox));
    EXPECT_TRUE(laovdb::tools::anyActiveTiles( tree, bbox));
    tiles = laovdb::tools::activeTiles(tree, bbox);
    EXPECT_TRUE( tiles.size() != 0u );
    for (auto &t : tiles) {
        EXPECT_TRUE( t.level == 1);
        EXPECT_TRUE( t.bbox.volume() == laovdb::math::Pow3(uint64_t(8)) );
        //std::cerr << "bbox = " << t.bbox << ", level = " << t.level << std::endl;
    }

    tree.denseFill(bbox, 1.0f);
    EXPECT_TRUE(laovdb::tools::anyActiveValues(tree, bbox));
    EXPECT_TRUE(laovdb::tools::anyActiveVoxels(tree, bbox));
    EXPECT_TRUE(!laovdb::tools::anyActiveTiles(tree, bbox));
    tiles = laovdb::tools::activeTiles(tree, bbox);
    EXPECT_TRUE( tiles.size() == 0u );
}

TEST_F(TestFindActiveValues, testSphere1)
{
    const laovdb::Vec3f center(0.5f, 0.5f, 0.5f);
    const float radius = 0.3f;
    const int dim = 100, half_width = 3;
    const float voxel_size = 1.0f/dim;

    laovdb::FloatGrid::Ptr grid = laovdb::FloatGrid::create(/*background=*/half_width*voxel_size);
    const laovdb::FloatTree& tree = grid->tree();
    grid->setTransform(laovdb::math::Transform::createLinearTransform(/*voxel size=*/voxel_size));
    unittest_util::makeSphere<laovdb::FloatGrid>(
        laovdb::Coord(dim), center, radius, *grid, unittest_util::SPHERE_SPARSE_NARROW_BAND);

    const int c = int(0.5f/voxel_size);
    const laovdb::CoordBBox a(laovdb::Coord(c), laovdb::Coord(c+ 8));
    EXPECT_TRUE(!tree.isValueOn(laovdb::Coord(c)));
    EXPECT_TRUE(!laovdb::tools::anyActiveValues(tree, a));

    const laovdb::Coord d(c + int(radius/voxel_size), c, c);
    EXPECT_TRUE(tree.isValueOn(d));
    const auto b = laovdb::CoordBBox::createCube(d, 4);
    EXPECT_TRUE(laovdb::tools::anyActiveValues(tree, b));

    const laovdb::CoordBBox e(laovdb::Coord(0), laovdb::Coord(dim));
    EXPECT_TRUE(laovdb::tools::anyActiveValues(tree, e));
    EXPECT_TRUE(!laovdb::tools::anyActiveTiles(tree, e));

    auto tiles = laovdb::tools::activeTiles(tree, e);
    EXPECT_TRUE( tiles.size() == 0u );
}

TEST_F(TestFindActiveValues, testSphere2)
{
    const laovdb::Vec3f center(0.0f);
    const float radius = 0.5f;
    const int dim = 400, halfWidth = 3;
    const float voxelSize = 2.0f/dim;
    auto grid  = laovdb::tools::createLevelSetSphere<laovdb::FloatGrid>(radius, center, voxelSize, halfWidth);
    laovdb::FloatTree& tree = grid->tree();

    {//test center
        const laovdb::CoordBBox bbox(laovdb::Coord(0), laovdb::Coord(8));
        EXPECT_TRUE(!tree.isValueOn(laovdb::Coord(0)));
        //laovdb::util::CpuTimer timer("\ncenter");
        EXPECT_TRUE(!laovdb::tools::anyActiveValues(tree, bbox));
        //timer.stop();
    }
    {//test on sphere
        const laovdb::Coord d(int(radius/voxelSize), 0, 0);
        EXPECT_TRUE(tree.isValueOn(d));
        const auto bbox = laovdb::CoordBBox::createCube(d, 4);
        //laovdb::util::CpuTimer timer("\non sphere");
        EXPECT_TRUE(laovdb::tools::anyActiveValues(tree, bbox));
        //timer.stop();
    }
    {//test full domain
        const laovdb::CoordBBox bbox(laovdb::Coord(-4000), laovdb::Coord(4000));
        //laovdb::util::CpuTimer timer("\nfull domain");
        EXPECT_TRUE(laovdb::tools::anyActiveValues(tree, bbox));
        //timer.stop();
        laovdb::tools::FindActiveValues<laovdb::FloatTree> op(tree);
        EXPECT_TRUE(op.count(bbox) == tree.activeVoxelCount());
    }
    {// find largest inscribed cube in index space containing NO active values
        laovdb::tools::FindActiveValues<laovdb::FloatTree> op(tree);
        auto bbox = laovdb::CoordBBox::createCube(laovdb::Coord(0), 1);
        //laovdb::util::CpuTimer timer("\nInscribed cube (class)");
        int count = 0;
        while(op.noActiveValues(bbox)) {
            ++count;
            bbox.expand(1);
        }
        //const double t = timer.stop();
        //std::cerr << "Inscribed bbox = " << bbox << std::endl;
        const int n = int(laovdb::math::Sqrt(laovdb::math::Pow2(radius-halfWidth*voxelSize)/3.0f)/voxelSize) + 1;
        //std::cerr << "n=" << n << std::endl;
        EXPECT_TRUE( bbox.max() == laovdb::Coord( n));
        EXPECT_TRUE( bbox.min() == laovdb::Coord(-n));
        //laovdb::util::printTime(std::cerr, t/count, "time per lookup ", "\n", true, 4, 3);
    }
    {// find largest inscribed cube in index space containing NO active values
        auto bbox = laovdb::CoordBBox::createCube(laovdb::Coord(0), 1);
        //laovdb::util::CpuTimer timer("\nInscribed cube (func)");
        int count = 0;
        while(!laovdb::tools::anyActiveValues(tree, bbox)) {
            bbox.expand(1);
            ++count;
        }
        //const double t = timer.stop();
        //std::cerr << "Inscribed bbox = " << bbox << std::endl;
        const int n = int(laovdb::math::Sqrt(laovdb::math::Pow2(radius-halfWidth*voxelSize)/3.0f)/voxelSize) + 1;
        //std::cerr << "n=" << n << std::endl;
        //laovdb::util::printTime(std::cerr, t/count, "time per lookup ", "\n", true, 4, 3);
        EXPECT_TRUE( bbox.max() == laovdb::Coord( n));
        EXPECT_TRUE( bbox.min() == laovdb::Coord(-n));
    }
}

TEST_F(TestFindActiveValues, testSparseBox)
{
    {//test active tiles in a sparsely filled box
        const int half_dim = 256;
        const laovdb::CoordBBox bbox(laovdb::Coord(-half_dim), laovdb::Coord(half_dim-1));
        laovdb::FloatTree tree;
        EXPECT_TRUE(tree.activeTileCount() == 0);
        EXPECT_TRUE(tree.getValueDepth(laovdb::Coord(0)) == -1);//background value
        laovdb::tools::FindActiveValues<laovdb::FloatTree> op(tree);

        tree.sparseFill(bbox, 1.0f, true);

        op.update(tree);//tree was modified so op needs to be updated
        EXPECT_TRUE(tree.activeTileCount() > 0);
        EXPECT_TRUE(tree.getValueDepth(laovdb::Coord(0)) == 1);//upper internal tile value
        for (int i=1; i<half_dim; ++i) {
            EXPECT_TRUE( op.anyActiveValues(laovdb::CoordBBox::createCube(laovdb::Coord(-half_dim), i)));
            EXPECT_TRUE(!op.anyActiveVoxels(laovdb::CoordBBox::createCube(laovdb::Coord(-half_dim), i)));
        }
        EXPECT_TRUE(op.count(bbox) == bbox.volume());

        auto bbox2 = laovdb::CoordBBox::createCube(laovdb::Coord(-half_dim), 1);
        //double t = 0.0;
        //laovdb::util::CpuTimer timer;
        for (bool test = true; test; ) {
            //timer.restart();
            test = op.anyActiveValues(bbox2);
            //t = std::max(t, timer.restart());
            if (test) bbox2.translate(laovdb::Coord(1));
        }
        //std::cerr << "bbox = " << bbox2 << std::endl;
        //laovdb::util::printTime(std::cout, t, "The slowest sparse test ", "\n", true, 4, 3);
        EXPECT_TRUE(bbox2 == laovdb::CoordBBox::createCube(laovdb::Coord(half_dim), 1));

        EXPECT_TRUE( laovdb::tools::anyActiveTiles(tree, bbox) );

        auto tiles = laovdb::tools::activeTiles(tree, bbox);
        EXPECT_TRUE( tiles.size() == laovdb::math::Pow3(size_t(4)) ); // {-256, -129} -> {-128, 0} -> {0, 127} -> {128, 255}
        //std::cerr << "bbox " << bbox << " overlaps with " << tiles.size() << " active tiles " << std::endl;
        laovdb::CoordBBox tmp;
        for (auto &t : tiles) {
            EXPECT_TRUE( t.state );
            EXPECT_TRUE( t.level == 2);// tiles at level 1 are 8^3, at level 2 they are 128^3, and at level 3 they are 4096^3
            EXPECT_TRUE( t.value == 1.0f);
            EXPECT_TRUE( t.bbox.volume() == laovdb::math::Pow3(laovdb::Index64(128)) );
            tmp.expand( t.bbox );
            //std::cerr << t.bbox << std::endl;
        }
        //std::cerr << tmp << std::endl;
        EXPECT_TRUE( tmp == bbox );// uniion of all the active tiles should equal the bbox of the sparseFill operation!
    }
}// testSparseBox

TEST_F(TestFindActiveValues, testDenseBox)
{
     {//test active voxels in a densely filled box
      const int half_dim = 256;
      const laovdb::CoordBBox bbox(laovdb::Coord(-half_dim), laovdb::Coord(half_dim));
      laovdb::FloatTree tree;

      EXPECT_TRUE(tree.activeTileCount() == 0);
      EXPECT_TRUE(tree.getValueDepth(laovdb::Coord(0)) == -1);//background value

      tree.denseFill(bbox, 1.0f, true);

      EXPECT_TRUE(tree.activeTileCount() == 0);

      laovdb::tools::FindActiveValues<laovdb::FloatTree> op(tree);
      EXPECT_TRUE(tree.getValueDepth(laovdb::Coord(0)) == 3);// leaf value
      for (int i=1; i<half_dim; ++i) {
          EXPECT_TRUE(op.anyActiveValues(laovdb::CoordBBox::createCube(laovdb::Coord(0), i)));
          EXPECT_TRUE(op.anyActiveVoxels(laovdb::CoordBBox::createCube(laovdb::Coord(0), i)));
      }
      EXPECT_TRUE(op.count(bbox) == bbox.volume());

      auto bbox2 = laovdb::CoordBBox::createCube(laovdb::Coord(-half_dim), 1);
      //double t = 0.0;
      //laovdb::util::CpuTimer timer;
      for (bool test = true; test; ) {
          //timer.restart();
          test = op.anyActiveValues(bbox2);
          //t = std::max(t, timer.restart());
          if (test) bbox2.translate(laovdb::Coord(1));
      }
      //std::cerr << "bbox = " << bbox2 << std::endl;
      //laovdb::util::printTime(std::cout, t, "The slowest dense test ", "\n", true, 4, 3);
      EXPECT_TRUE(bbox2 == laovdb::CoordBBox::createCube(laovdb::Coord(half_dim + 1), 1));

      auto tiles = laovdb::tools::activeTiles(tree, bbox);
      EXPECT_TRUE( tiles.size() == 0u );
    }
}// testDenseBox

TEST_F(TestFindActiveValues, testBenchmarks)
{
    {//benchmark test against active tiles in a sparsely filled box
      using namespace laovdb;
      const int half_dim = 512, bbox_size = 6;
      const CoordBBox bbox(Coord(-half_dim), Coord(half_dim));
      FloatTree tree;
      tree.sparseFill(bbox, 1.0f, true);
      tools::FindActiveValues<FloatTree> op(tree);
      //double t = 0.0;
      //util::CpuTimer timer;
      for (auto b = CoordBBox::createCube(Coord(-half_dim), bbox_size); true; b.translate(Coord(1))) {
          //timer.restart();
          bool test = op.anyActiveValues(b);
          //t = std::max(t, timer.restart());
          if (!test) break;
      }
      //std::cout << "\n*The slowest sparse test " << t << " milliseconds\n";
      EXPECT_TRUE(op.count(bbox) == bbox.volume());
    }
    {//benchmark test against active voxels in a densely filled box
      using namespace laovdb;
      const int half_dim = 256, bbox_size = 1;
      const CoordBBox bbox(Coord(-half_dim), Coord(half_dim));
      FloatTree tree;
      tree.denseFill(bbox, 1.0f, true);
      tools::FindActiveValues<FloatTree> op(tree);
      //double t = 0.0;
      //laovdb::util::CpuTimer timer;
      for (auto b = CoordBBox::createCube(Coord(-half_dim), bbox_size); true; b.translate(Coord(1))) {
          //timer.restart();
          bool test = op.anyActiveValues(b);
          //t = std::max(t, timer.restart());
          if (!test) break;
      }
      //std::cout << "*The slowest dense test " << t << " milliseconds\n";
      EXPECT_TRUE(op.count(bbox) == bbox.volume());
    }
    {//benchmark test against active voxels in a densely filled box
      using namespace laovdb;
      FloatTree tree;
      tree.denseFill(CoordBBox::createCube(Coord(0), 256), 1.0f, true);
      tools::FindActiveValues<FloatTree> op(tree);
      //laovdb::util::CpuTimer timer("new test");
      EXPECT_TRUE(op.noActiveValues(CoordBBox::createCube(Coord(256), 1)));
      //timer.stop();
    }
}// testBenchmarks
