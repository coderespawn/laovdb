// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

#include <openvdb/Exceptions.h>
#include <openvdb/tree/LeafNode.h>
#include <openvdb/Types.h>
#include <gtest/gtest.h>

#include <cctype> // for toupper()
#include <iostream>
#include <sstream>

template<typename T>
class TestLeafIO
{
public:
    static void testBuffer();
};

template<typename T>
void
TestLeafIO<T>::testBuffer()
{
    laovdb::tree::LeafNode<T, 3> leaf(laovdb::Coord(0, 0, 0));

    leaf.setValueOn(laovdb::Coord(0, 1, 0), T(1));
    leaf.setValueOn(laovdb::Coord(1, 0, 0), T(1));

    std::ostringstream ostr(std::ios_base::binary);

    leaf.writeBuffers(ostr);

    leaf.setValueOn(laovdb::Coord(0, 1, 0), T(0));
    leaf.setValueOn(laovdb::Coord(0, 1, 1), T(1));

    std::istringstream istr(ostr.str(), std::ios_base::binary);

    // Since the input stream doesn't include a VDB header with file format version info,
    // tag the input stream explicitly with the current version number.
    laovdb::io::setCurrentVersion(istr);

    leaf.readBuffers(istr);

    EXPECT_NEAR(T(1), leaf.getValue(laovdb::Coord(0, 1, 0)), /*tolerance=*/0);
    EXPECT_NEAR(T(1), leaf.getValue(laovdb::Coord(1, 0, 0)), /*tolerance=*/0);

    EXPECT_TRUE(leaf.onVoxelCount() == 2);
}


class TestLeafIOTest: public ::testing::Test
{
};


TEST_F(TestLeafIOTest, testBufferInt) { TestLeafIO<int>::testBuffer(); }
TEST_F(TestLeafIOTest, testBufferFloat) { TestLeafIO<float>::testBuffer(); }
TEST_F(TestLeafIOTest, testBufferDouble) { TestLeafIO<double>::testBuffer(); }
TEST_F(TestLeafIOTest, testBufferBool) { TestLeafIO<bool>::testBuffer(); }
TEST_F(TestLeafIOTest, testBufferByte) { TestLeafIO<laovdb::Byte>::testBuffer(); }

TEST_F(TestLeafIOTest, testBufferString)
{
    laovdb::tree::LeafNode<std::string, 3>
        leaf(laovdb::Coord(0, 0, 0), std::string());

    leaf.setValueOn(laovdb::Coord(0, 1, 0), std::string("test"));
    leaf.setValueOn(laovdb::Coord(1, 0, 0), std::string("test"));

    std::ostringstream ostr(std::ios_base::binary);

    leaf.writeBuffers(ostr);

    leaf.setValueOn(laovdb::Coord(0, 1, 0), std::string("douche"));
    leaf.setValueOn(laovdb::Coord(0, 1, 1), std::string("douche"));

    std::istringstream istr(ostr.str(), std::ios_base::binary);

    // Since the input stream doesn't include a VDB header with file format version info,
    // tag the input stream explicitly with the current version number.
    laovdb::io::setCurrentVersion(istr);

    leaf.readBuffers(istr);

    EXPECT_EQ(std::string("test"), leaf.getValue(laovdb::Coord(0, 1, 0)));
    EXPECT_EQ(std::string("test"), leaf.getValue(laovdb::Coord(1, 0, 0)));

    EXPECT_TRUE(leaf.onVoxelCount() == 2);
}


TEST_F(TestLeafIOTest, testBufferVec3R)
{
    laovdb::tree::LeafNode<laovdb::Vec3R, 3> leaf(laovdb::Coord(0, 0, 0));

    leaf.setValueOn(laovdb::Coord(0, 1, 0), laovdb::Vec3R(1, 1, 1));
    leaf.setValueOn(laovdb::Coord(1, 0, 0), laovdb::Vec3R(1, 1, 1));

    std::ostringstream ostr(std::ios_base::binary);

    leaf.writeBuffers(ostr);

    leaf.setValueOn(laovdb::Coord(0, 1, 0), laovdb::Vec3R(0, 0, 0));
    leaf.setValueOn(laovdb::Coord(0, 1, 1), laovdb::Vec3R(1, 1, 1));

    std::istringstream istr(ostr.str(), std::ios_base::binary);

    // Since the input stream doesn't include a VDB header with file format version info,
    // tag the input stream explicitly with the current version number.
    laovdb::io::setCurrentVersion(istr);

    leaf.readBuffers(istr);

    EXPECT_TRUE(leaf.getValue(laovdb::Coord(0, 1, 0)) == laovdb::Vec3R(1, 1, 1));
    EXPECT_TRUE(leaf.getValue(laovdb::Coord(1, 0, 0)) == laovdb::Vec3R(1, 1, 1));

    EXPECT_TRUE(leaf.onVoxelCount() == 2);
}
