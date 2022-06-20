// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

#include <openvdb/Types.h>
#include <openvdb/math/Coord.h>

#include <gtest/gtest.h>

#include <unordered_map>
#include <sstream>


class TestCoord: public ::testing::Test
{
};


TEST_F(TestCoord, testCoord)
{
    using laovdb::Coord;

    for (int i=0; i<3; ++i) {
        EXPECT_EQ(Coord::min()[i], std::numeric_limits<Coord::Int32>::min());
        EXPECT_EQ(Coord::max()[i], std::numeric_limits<Coord::Int32>::max());
    }

    Coord xyz(-1, 2, 4);
    Coord xyz2 = -xyz;
    EXPECT_EQ(Coord(1, -2, -4), xyz2);

    EXPECT_EQ(Coord(1, 2, 4), laovdb::math::Abs(xyz));

    xyz2 = -xyz2;
    EXPECT_EQ(xyz, xyz2);

    xyz.setX(-xyz.x());
    EXPECT_EQ(Coord(1, 2, 4), xyz);

    xyz2 = xyz >> 1;
    EXPECT_EQ(Coord(0, 1, 2), xyz2);

    xyz2 |= 1;
    EXPECT_EQ(Coord(1, 1, 3), xyz2);

    EXPECT_TRUE(xyz2 != xyz);
    EXPECT_TRUE(xyz2 < xyz);
    EXPECT_TRUE(xyz2 <= xyz);

    Coord xyz3(xyz2);
    xyz2 -= xyz3;
    EXPECT_EQ(Coord(), xyz2);

    xyz2.reset(0, 4, 4);
    xyz2.offset(-1);
    EXPECT_EQ(Coord(-1, 3, 3), xyz2);

    // xyz = (1, 2, 4), xyz2 = (-1, 3, 3)
    EXPECT_EQ(Coord(-1, 2, 3), Coord::minComponent(xyz, xyz2));
    EXPECT_EQ(Coord(1, 3, 4), Coord::maxComponent(xyz, xyz2));
}


TEST_F(TestCoord, testConversion)
{
    using laovdb::Coord;

    laovdb::Vec3I iv(1, 2, 4);
    Coord xyz(iv);
    EXPECT_EQ(Coord(1, 2, 4), xyz);
    EXPECT_EQ(iv, xyz.asVec3I());
    EXPECT_EQ(laovdb::Vec3i(1, 2, 4), xyz.asVec3i());

    iv = (xyz + iv) + xyz;
    EXPECT_EQ(laovdb::Vec3I(3, 6, 12), iv);
    iv = iv - xyz;
    EXPECT_EQ(laovdb::Vec3I(2, 4, 8), iv);

    laovdb::Vec3s fv = xyz.asVec3s();
    EXPECT_TRUE(laovdb::math::isExactlyEqual(laovdb::Vec3s(1, 2, 4), fv));
}


TEST_F(TestCoord, testIO)
{
    using laovdb::Coord;

    Coord xyz(-1, 2, 4), xyz2;

    std::ostringstream os(std::ios_base::binary);
    EXPECT_NO_THROW(xyz.write(os));

    std::istringstream is(os.str(), std::ios_base::binary);
    EXPECT_NO_THROW(xyz2.read(is));

    EXPECT_EQ(xyz, xyz2);

    os.str("");
    os << xyz;
    EXPECT_EQ(std::string("[-1, 2, 4]"), os.str());
}

TEST_F(TestCoord, testCoordBBox)
{
    {// Empty constructor
        laovdb::CoordBBox b;
        EXPECT_EQ(laovdb::Coord::max(), b.min());
        EXPECT_EQ(laovdb::Coord::min(), b.max());
        EXPECT_TRUE(b.empty());
    }
    {// Construct bbox from min and max
        const laovdb::Coord min(-1,-2,30), max(20,30,55);
        laovdb::CoordBBox b(min, max);
        EXPECT_EQ(min, b.min());
        EXPECT_EQ(max, b.max());
    }
    {// Construct bbox from components of min and max
        const laovdb::Coord min(-1,-2,30), max(20,30,55);
        laovdb::CoordBBox b(min[0], min[1], min[2],
                             max[0], max[1], max[2]);
        EXPECT_EQ(min, b.min());
        EXPECT_EQ(max, b.max());
    }
    {// tbb::split constructor
         const laovdb::Coord min(-1,-2,30), max(20,30,55);
         laovdb::CoordBBox a(min, max), b(a, tbb::split());
         EXPECT_EQ(min, b.min());
         EXPECT_EQ(laovdb::Coord(20, 14, 55), b.max());
         EXPECT_EQ(laovdb::Coord(-1, 15, 30), a.min());
         EXPECT_EQ(max, a.max());
    }
    {// createCube
        const laovdb::Coord min(0,8,16);
        const laovdb::CoordBBox b = laovdb::CoordBBox::createCube(min, 8);
        EXPECT_EQ(min, b.min());
        EXPECT_EQ(min + laovdb::Coord(8-1), b.max());
    }
    {// inf
        const laovdb::CoordBBox b = laovdb::CoordBBox::inf();
        EXPECT_EQ(laovdb::Coord::min(), b.min());
        EXPECT_EQ(laovdb::Coord::max(), b.max());
    }
    {// empty, dim, hasVolume and volume
        const laovdb::Coord c(1,2,3);
        const laovdb::CoordBBox b0(c, c), b1(c, c.offsetBy(0,-1,0)), b2;
        EXPECT_TRUE( b0.hasVolume() && !b0.empty());
        EXPECT_TRUE(!b1.hasVolume() &&  b1.empty());
        EXPECT_TRUE(!b2.hasVolume() &&  b2.empty());
        EXPECT_EQ(laovdb::Coord(1), b0.dim());
        EXPECT_EQ(laovdb::Coord(0), b1.dim());
        EXPECT_EQ(laovdb::Coord(0), b2.dim());
        EXPECT_EQ(uint64_t(1), b0.volume());
        EXPECT_EQ(uint64_t(0), b1.volume());
        EXPECT_EQ(uint64_t(0), b2.volume());
    }
    {// volume and split constructor
        const laovdb::Coord min(-1,-2,30), max(20,30,55);
        const laovdb::CoordBBox bbox(min,max);
        laovdb::CoordBBox a(bbox), b(a, tbb::split());
        EXPECT_EQ(bbox.volume(), a.volume() + b.volume());
        laovdb::CoordBBox c(b, tbb::split());
        EXPECT_EQ(bbox.volume(), a.volume() + b.volume() + c.volume());
    }
    {// getCenter
        const laovdb::Coord min(1,2,3), max(6,10,15);
        const laovdb::CoordBBox b(min, max);
        EXPECT_EQ(laovdb::Vec3d(3.5, 6.0, 9.0), b.getCenter());
    }
    {// moveMin
        const laovdb::Coord min(1,2,3), max(6,10,15);
        laovdb::CoordBBox b(min, max);
        const laovdb::Coord dim = b.dim();
        b.moveMin(laovdb::Coord(0));
        EXPECT_EQ(dim, b.dim());
        EXPECT_EQ(laovdb::Coord(0), b.min());
        EXPECT_EQ(max-min, b.max());
    }
    {// moveMax
        const laovdb::Coord min(1,2,3), max(6,10,15);
        laovdb::CoordBBox b(min, max);
        const laovdb::Coord dim = b.dim();
        b.moveMax(laovdb::Coord(0));
        EXPECT_EQ(dim, b.dim());
        EXPECT_EQ(laovdb::Coord(0), b.max());
        EXPECT_EQ(min-max, b.min());
    }
    {// a volume that overflows Int32.
        using Int32 = laovdb::Int32;
        Int32 maxInt32 = std::numeric_limits<Int32>::max();
        const laovdb::Coord min(Int32(0), Int32(0), Int32(0));
        const laovdb::Coord max(maxInt32-Int32(2), Int32(2), Int32(2));

        const laovdb::CoordBBox b(min, max);
        uint64_t volume = UINT64_C(19327352814);
        EXPECT_EQ(volume, b.volume());
    }
    {// minExtent and maxExtent
        const laovdb::Coord min(1,2,3);
        {
            const laovdb::Coord max = min + laovdb::Coord(1,2,3);
            const laovdb::CoordBBox b(min, max);
            EXPECT_EQ(int(b.minExtent()), 0);
            EXPECT_EQ(int(b.maxExtent()), 2);
        }
        {
            const laovdb::Coord max = min + laovdb::Coord(1,3,2);
            const laovdb::CoordBBox b(min, max);
            EXPECT_EQ(int(b.minExtent()), 0);
            EXPECT_EQ(int(b.maxExtent()), 1);
        }
        {
            const laovdb::Coord max = min + laovdb::Coord(2,1,3);
            const laovdb::CoordBBox b(min, max);
            EXPECT_EQ(int(b.minExtent()), 1);
            EXPECT_EQ(int(b.maxExtent()), 2);
        }
        {
            const laovdb::Coord max = min + laovdb::Coord(2,3,1);
            const laovdb::CoordBBox b(min, max);
            EXPECT_EQ(int(b.minExtent()), 2);
            EXPECT_EQ(int(b.maxExtent()), 1);
        }
        {
            const laovdb::Coord max = min + laovdb::Coord(3,1,2);
            const laovdb::CoordBBox b(min, max);
            EXPECT_EQ(int(b.minExtent()), 1);
            EXPECT_EQ(int(b.maxExtent()), 0);
        }
        {
            const laovdb::Coord max = min + laovdb::Coord(3,2,1);
            const laovdb::CoordBBox b(min, max);
            EXPECT_EQ(int(b.minExtent()), 2);
            EXPECT_EQ(int(b.maxExtent()), 0);
        }
    }

    {//reset
        laovdb::CoordBBox b;
        EXPECT_EQ(laovdb::Coord::max(), b.min());
        EXPECT_EQ(laovdb::Coord::min(), b.max());
        EXPECT_TRUE(b.empty());

        const laovdb::Coord min(-1,-2,30), max(20,30,55);
        b.reset(min, max);
        EXPECT_EQ(min, b.min());
        EXPECT_EQ(max, b.max());
        EXPECT_TRUE(!b.empty());

        b.reset();
        EXPECT_EQ(laovdb::Coord::max(), b.min());
        EXPECT_EQ(laovdb::Coord::min(), b.max());
        EXPECT_TRUE(b.empty());
    }

    {// ZYX Iterator 1
        const laovdb::Coord min(-1,-2,3), max(2,3,5);
        const laovdb::CoordBBox b(min, max);
        const size_t count = b.volume();
        size_t n = 0;
        laovdb::CoordBBox::ZYXIterator ijk(b);
        for (int i=min[0]; i<=max[0]; ++i) {
            for (int j=min[1]; j<=max[1]; ++j) {
                for (int k=min[2]; k<=max[2]; ++k, ++ijk, ++n) {
                    EXPECT_TRUE(ijk);
                    EXPECT_EQ(laovdb::Coord(i,j,k), *ijk);
                }
            }
        }
        EXPECT_EQ(count, n);
        EXPECT_TRUE(!ijk);
        ++ijk;
        EXPECT_TRUE(!ijk);
    }

    {// ZYX Iterator 2
        const laovdb::Coord min(-1,-2,3), max(2,3,5);
        const laovdb::CoordBBox b(min, max);
        const size_t count = b.volume();
        size_t n = 0;
        laovdb::Coord::ValueType unused = 0;
        for (const auto& ijk: b) {
            unused += ijk[0];
            EXPECT_TRUE(++n <= count);
        }
        EXPECT_EQ(count, n);
    }

    {// XYZ Iterator 1
        const laovdb::Coord min(-1,-2,3), max(2,3,5);
        const laovdb::CoordBBox b(min, max);
        const size_t count = b.volume();
        size_t n = 0;
        laovdb::CoordBBox::XYZIterator ijk(b);
        for (int k=min[2]; k<=max[2]; ++k) {
            for (int j=min[1]; j<=max[1]; ++j) {
                for (int i=min[0]; i<=max[0]; ++i, ++ijk, ++n) {
                    EXPECT_TRUE( ijk );
                    EXPECT_EQ( laovdb::Coord(i,j,k), *ijk );
                }
            }
        }
        EXPECT_EQ(count, n);
        EXPECT_TRUE( !ijk );
        ++ijk;
        EXPECT_TRUE( !ijk );
    }

    {// XYZ Iterator 2
        const laovdb::Coord min(-1,-2,3), max(2,3,5);
        const laovdb::CoordBBox b(min, max);
        const size_t count = b.volume();
        size_t n = 0;
        for (auto ijk = b.beginXYZ(); ijk; ++ijk) {
            EXPECT_TRUE( ++n <= count );
        }
        EXPECT_EQ(count, n);
    }

    {// bit-wise operations (note that the API doesn't define behaviour for shifting neg coords)
        const laovdb::Coord min(1,2,3), max(2,3,5);
        const laovdb::CoordBBox b(min, max);
        EXPECT_EQ(laovdb::CoordBBox(min>>1,max>>1), b>>size_t(1));
        EXPECT_EQ(laovdb::CoordBBox(min>>3,max>>3), b>>size_t(3));
        EXPECT_EQ(laovdb::CoordBBox(min<<1,max<<1), b<<size_t(1));
        EXPECT_EQ(laovdb::CoordBBox(min&1,max&1), b&1);
        EXPECT_EQ(laovdb::CoordBBox(min|1,max|1), b|1);
    }

    {// test getCornerPoints
        const laovdb::CoordBBox bbox(1, 2, 3, 4, 5, 6);
        laovdb::Coord a[10];
        bbox.getCornerPoints(a);
        //for (int i=0; i<8; ++i) {
        //    std::cerr << "#"<<i<<" = ("<<a[i][0]<<","<<a[i][1]<<","<<a[i][2]<<")\n";
        //}
        EXPECT_EQ( a[0], laovdb::Coord(1, 2, 3) );
        EXPECT_EQ( a[1], laovdb::Coord(1, 2, 6) );
        EXPECT_EQ( a[2], laovdb::Coord(1, 5, 3) );
        EXPECT_EQ( a[3], laovdb::Coord(1, 5, 6) );
        EXPECT_EQ( a[4], laovdb::Coord(4, 2, 3) );
        EXPECT_EQ( a[5], laovdb::Coord(4, 2, 6) );
        EXPECT_EQ( a[6], laovdb::Coord(4, 5, 3) );
        EXPECT_EQ( a[7], laovdb::Coord(4, 5, 6) );
        for (int i=1; i<8; ++i) EXPECT_TRUE( a[i-1] < a[i] );
    }
}

TEST_F(TestCoord, testCoordHash)
{
    {//test Coord::hash function
      laovdb::Coord a(-1, 34, 67), b(-2, 34, 67);
      EXPECT_TRUE(a.hash<>() != b.hash<>());
      EXPECT_TRUE(a.hash<10>() != b.hash<10>());
      EXPECT_TRUE(a.hash<5>() != b.hash<5>());
    }

    {//test std::hash function
      std::hash<laovdb::Coord> h;
      laovdb::Coord a(-1, 34, 67), b(-2, 34, 67);
      EXPECT_TRUE(h(a) != h(b));
    }

    {//test hash map (= unordered_map)
      using KeyT = laovdb::Coord;
      using ValueT = size_t;
      using HashT = std::hash<laovdb::Coord>;

      std::unordered_map<KeyT, ValueT, HashT> h;
      const laovdb::Coord min(-10,-20,30), max(20,30,50);
      const laovdb::CoordBBox bbox(min, max);
      size_t n = 0;
      for (const auto& ijk: bbox) h[ijk] = n++;
      EXPECT_EQ(h.size(), n);
      n = 0;
      for (const auto& ijk: bbox) EXPECT_EQ(h[ijk], n++);
      EXPECT_TRUE(h.load_factor() <= 1.0f);// no hask key collisions!
    }
}
