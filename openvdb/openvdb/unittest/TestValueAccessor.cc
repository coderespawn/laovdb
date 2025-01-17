// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

#include <openvdb/openvdb.h>
#include <openvdb/tools/Prune.h>

#include <gtest/gtest.h>
#include <tbb/task_group.h>

#include <type_traits>

#define ASSERT_DOUBLES_EXACTLY_EQUAL(expected, actual) \
    EXPECT_NEAR((expected), (actual), /*tolerance=*/0.0);


using ValueType = float;
using Tree2Type = laovdb::tree::Tree<
    laovdb::tree::RootNode<
    laovdb::tree::LeafNode<ValueType, 3> > >;
using Tree3Type = laovdb::tree::Tree<
    laovdb::tree::RootNode<
    laovdb::tree::InternalNode<
    laovdb::tree::LeafNode<ValueType, 3>, 4> > >;
using Tree4Type = laovdb::tree::Tree4<ValueType, 5, 4, 3>::Type;
using Tree5Type = laovdb::tree::Tree<
    laovdb::tree::RootNode<
    laovdb::tree::InternalNode<
    laovdb::tree::InternalNode<
    laovdb::tree::InternalNode<
    laovdb::tree::LeafNode<ValueType, 3>, 4>, 5>, 5> > >;
using TreeType = Tree4Type;


using namespace laovdb::tree;

class TestValueAccessor: public ::testing::Test
{
public:
    void SetUp() override { laovdb::initialize(); }
    void TearDown() override { laovdb::uninitialize(); }

    // Test odd combinations of trees and ValueAccessors
    // cache node level 0 and 1
    void testTree3Accessor2()
    {
        accessorTest<ValueAccessor<Tree3Type, true,  2> >();
        accessorTest<ValueAccessor<Tree3Type, false, 2> >();
    }
    void testTree3ConstAccessor2()
    {
        constAccessorTest<ValueAccessor<const Tree3Type, true,  2> >();
        constAccessorTest<ValueAccessor<const Tree3Type, false, 2> >();
    }
    void testTree4Accessor2()
    {
        accessorTest<ValueAccessor<Tree4Type, true,  2> >();
        accessorTest<ValueAccessor<Tree4Type, false, 2> >();
    }
    void testTree4ConstAccessor2()
    {
        constAccessorTest<ValueAccessor<const Tree4Type, true,  2> >();
        constAccessorTest<ValueAccessor<const Tree4Type, false, 2> >();
    }
    void testTree5Accessor2()
    {
        accessorTest<ValueAccessor<Tree5Type, true,  2> >();
        accessorTest<ValueAccessor<Tree5Type, false, 2> >();
    }
    void testTree5ConstAccessor2()
    {
        constAccessorTest<ValueAccessor<const Tree5Type, true,  2> >();
        constAccessorTest<ValueAccessor<const Tree5Type, false, 2> >();
    }
    // only cache leaf level
    void testTree4Accessor1()
    {
        accessorTest<ValueAccessor<Tree5Type, true,  1> >();
        accessorTest<ValueAccessor<Tree5Type, false, 1> >();
    }
    void testTree4ConstAccessor1()
    {
        constAccessorTest<ValueAccessor<const Tree5Type, true,  1> >();
        constAccessorTest<ValueAccessor<const Tree5Type, false, 1> >();
    }
    // disable node caching
    void testTree4Accessor0()
    {
        accessorTest<ValueAccessor<Tree5Type, true,  0> >();
        accessorTest<ValueAccessor<Tree5Type, false, 0> >();
    }
    void testTree4ConstAccessor0()
    {
        constAccessorTest<ValueAccessor<const Tree5Type, true,  0> >();
        constAccessorTest<ValueAccessor<const Tree5Type, false, 0> >();
    }
    //cache node level 2
    void testTree4Accessor12()
    {
        accessorTest<ValueAccessor1<Tree4Type, true,  2> >();
        accessorTest<ValueAccessor1<Tree4Type, false, 2> >();
    }
    //cache node level 1 and 3
    void testTree5Accessor213()
    {
        accessorTest<ValueAccessor2<Tree5Type, true, 1,3> >();
        accessorTest<ValueAccessor2<Tree5Type, false, 1,3> >();
    }

protected:
    template<typename AccessorT> void accessorTest();
    template<typename AccessorT> void constAccessorTest();
};


////////////////////////////////////////


namespace {

struct Plus
{
    float addend;
    Plus(float f): addend(f) {}
    inline void operator()(float& f) const { f += addend; }
    inline void operator()(float& f, bool& b) const { f += addend; b = false; }
};

}


template<typename AccessorT>
void
TestValueAccessor::accessorTest()
{
    using TreeType = typename AccessorT::TreeType;
    const int leafDepth = int(TreeType::DEPTH) - 1;
    // subtract one because getValueDepth() returns 0 for values at the root

    const ValueType background = 5.0f, value = -9.345f;
    const laovdb::Coord c0(5, 10, 20), c1(500000, 200000, 300000);

    {
        TreeType tree(background);
        EXPECT_TRUE(!tree.isValueOn(c0));
        EXPECT_TRUE(!tree.isValueOn(c1));
        ASSERT_DOUBLES_EXACTLY_EQUAL(background, tree.getValue(c0));
        ASSERT_DOUBLES_EXACTLY_EQUAL(background, tree.getValue(c1));
        tree.setValue(c0, value);
        EXPECT_TRUE(tree.isValueOn(c0));
        EXPECT_TRUE(!tree.isValueOn(c1));
        ASSERT_DOUBLES_EXACTLY_EQUAL(value, tree.getValue(c0));
        ASSERT_DOUBLES_EXACTLY_EQUAL(background, tree.getValue(c1));
    }
    {
        TreeType tree(background);
        AccessorT acc(tree);
        ValueType v;

        EXPECT_TRUE(!tree.isValueOn(c0));
        EXPECT_TRUE(!tree.isValueOn(c1));
        ASSERT_DOUBLES_EXACTLY_EQUAL(background, tree.getValue(c0));
        ASSERT_DOUBLES_EXACTLY_EQUAL(background, tree.getValue(c1));
        EXPECT_TRUE(!acc.isCached(c0));
        EXPECT_TRUE(!acc.isCached(c1));
        EXPECT_TRUE(!acc.probeValue(c0,v));
        ASSERT_DOUBLES_EXACTLY_EQUAL(background, v);
        EXPECT_TRUE(!acc.probeValue(c1,v));
        ASSERT_DOUBLES_EXACTLY_EQUAL(background, v);
        EXPECT_EQ(-1, acc.getValueDepth(c0));
        EXPECT_EQ(-1, acc.getValueDepth(c1));
        EXPECT_TRUE(!acc.isVoxel(c0));
        EXPECT_TRUE(!acc.isVoxel(c1));

        acc.setValue(c0, value);

        EXPECT_TRUE(tree.isValueOn(c0));
        EXPECT_TRUE(!tree.isValueOn(c1));
        ASSERT_DOUBLES_EXACTLY_EQUAL(value, tree.getValue(c0));
        ASSERT_DOUBLES_EXACTLY_EQUAL(background, tree.getValue(c1));
        EXPECT_TRUE(acc.probeValue(c0,v));
        ASSERT_DOUBLES_EXACTLY_EQUAL(value, v);
        EXPECT_TRUE(!acc.probeValue(c1,v));
        ASSERT_DOUBLES_EXACTLY_EQUAL(background, v);
        EXPECT_EQ(leafDepth, acc.getValueDepth(c0)); // leaf-level voxel value
        EXPECT_EQ(-1, acc.getValueDepth(c1)); // background value
        EXPECT_EQ(leafDepth, acc.getValueDepth(laovdb::Coord(7, 10, 20)));
        const int depth = leafDepth == 1 ? -1 : leafDepth - 1;
        EXPECT_EQ(depth, acc.getValueDepth(laovdb::Coord(8, 10, 20)));
        EXPECT_TRUE( acc.isVoxel(c0)); // leaf-level voxel value
        EXPECT_TRUE(!acc.isVoxel(c1));
        EXPECT_TRUE( acc.isVoxel(laovdb::Coord(7, 10, 20)));
        EXPECT_TRUE(!acc.isVoxel(laovdb::Coord(8, 10, 20)));

        ASSERT_DOUBLES_EXACTLY_EQUAL(background, acc.getValue(c1));
        EXPECT_TRUE(!acc.isCached(c1)); // uncached background value
        EXPECT_TRUE(!acc.isValueOn(c1)); // inactive background value
        ASSERT_DOUBLES_EXACTLY_EQUAL(value, acc.getValue(c0));
        EXPECT_TRUE(
            (acc.numCacheLevels()>0) == acc.isCached(c0)); // active, leaf-level voxel value
        EXPECT_TRUE(acc.isValueOn(c0));

        acc.setValue(c1, value);

        EXPECT_TRUE(acc.isValueOn(c1));
        ASSERT_DOUBLES_EXACTLY_EQUAL(value, tree.getValue(c0));
        ASSERT_DOUBLES_EXACTLY_EQUAL(value, tree.getValue(c1));
        EXPECT_TRUE((acc.numCacheLevels()>0) == acc.isCached(c1));
        ASSERT_DOUBLES_EXACTLY_EQUAL(value, acc.getValue(c1));
        EXPECT_TRUE(!acc.isCached(c0));
        ASSERT_DOUBLES_EXACTLY_EQUAL(value, acc.getValue(c0));
        EXPECT_TRUE((acc.numCacheLevels()>0) == acc.isCached(c0));
        EXPECT_EQ(leafDepth, acc.getValueDepth(c0));
        EXPECT_EQ(leafDepth, acc.getValueDepth(c1));
        EXPECT_TRUE(acc.isVoxel(c0));
        EXPECT_TRUE(acc.isVoxel(c1));

        tree.setValueOff(c1);

        ASSERT_DOUBLES_EXACTLY_EQUAL(value, tree.getValue(c0));
        ASSERT_DOUBLES_EXACTLY_EQUAL(value, tree.getValue(c1));
        EXPECT_TRUE(!acc.isCached(c0));
        EXPECT_TRUE((acc.numCacheLevels()>0) == acc.isCached(c1));
        EXPECT_TRUE( acc.isValueOn(c0));
        EXPECT_TRUE(!acc.isValueOn(c1));

        acc.setValueOn(c1);

        EXPECT_TRUE(!acc.isCached(c0));
        EXPECT_TRUE((acc.numCacheLevels()>0) == acc.isCached(c1));
        EXPECT_TRUE( acc.isValueOn(c0));
        EXPECT_TRUE( acc.isValueOn(c1));

        acc.modifyValueAndActiveState(c1, Plus(-value)); // subtract value & mark inactive
        EXPECT_TRUE(!acc.isValueOn(c1));

        acc.modifyValue(c1, Plus(-value)); // subtract value again & mark active

        EXPECT_TRUE(acc.isValueOn(c1));
        ASSERT_DOUBLES_EXACTLY_EQUAL(value, tree.getValue(c0));
        ASSERT_DOUBLES_EXACTLY_EQUAL(-value, tree.getValue(c1));
        EXPECT_TRUE((acc.numCacheLevels()>0) == acc.isCached(c1));
        ASSERT_DOUBLES_EXACTLY_EQUAL(-value, acc.getValue(c1));
        EXPECT_TRUE(!acc.isCached(c0));
        ASSERT_DOUBLES_EXACTLY_EQUAL(value, acc.getValue(c0));
        EXPECT_TRUE((acc.numCacheLevels()>0) == acc.isCached(c0));
        EXPECT_EQ(leafDepth, acc.getValueDepth(c0));
        EXPECT_EQ(leafDepth, acc.getValueDepth(c1));
        EXPECT_TRUE(acc.isVoxel(c0));
        EXPECT_TRUE(acc.isVoxel(c1));

        acc.setValueOnly(c1, 3*value);

        EXPECT_TRUE(acc.isValueOn(c1));
        ASSERT_DOUBLES_EXACTLY_EQUAL(value, tree.getValue(c0));
        ASSERT_DOUBLES_EXACTLY_EQUAL(3*value, tree.getValue(c1));
        EXPECT_TRUE((acc.numCacheLevels()>0) == acc.isCached(c1));
        ASSERT_DOUBLES_EXACTLY_EQUAL(3*value, acc.getValue(c1));
        EXPECT_TRUE(!acc.isCached(c0));
        ASSERT_DOUBLES_EXACTLY_EQUAL(value, acc.getValue(c0));
        EXPECT_TRUE((acc.numCacheLevels()>0) == acc.isCached(c0));
        EXPECT_EQ(leafDepth, acc.getValueDepth(c0));
        EXPECT_EQ(leafDepth, acc.getValueDepth(c1));
        EXPECT_TRUE(acc.isVoxel(c0));
        EXPECT_TRUE(acc.isVoxel(c1));

        acc.clear();
        EXPECT_TRUE(!acc.isCached(c0));
        EXPECT_TRUE(!acc.isCached(c1));
    }
}


template<typename AccessorT>
void
TestValueAccessor::constAccessorTest()
{
    using TreeType = typename std::remove_const<typename AccessorT::TreeType>::type;
    const int leafDepth = int(TreeType::DEPTH) - 1;
        // subtract one because getValueDepth() returns 0 for values at the root

    const ValueType background = 5.0f, value = -9.345f;
    const laovdb::Coord c0(5, 10, 20), c1(500000, 200000, 300000);
    ValueType v;

    TreeType tree(background);
    AccessorT acc(tree);

    EXPECT_TRUE(!tree.isValueOn(c0));
    EXPECT_TRUE(!tree.isValueOn(c1));
    ASSERT_DOUBLES_EXACTLY_EQUAL(background, tree.getValue(c0));
    ASSERT_DOUBLES_EXACTLY_EQUAL(background, tree.getValue(c1));
    EXPECT_TRUE(!acc.isCached(c0));
    EXPECT_TRUE(!acc.isCached(c1));
    EXPECT_TRUE(!acc.probeValue(c0,v));
    ASSERT_DOUBLES_EXACTLY_EQUAL(background, v);
    EXPECT_TRUE(!acc.probeValue(c1,v));
    ASSERT_DOUBLES_EXACTLY_EQUAL(background, v);
    EXPECT_EQ(-1, acc.getValueDepth(c0));
    EXPECT_EQ(-1, acc.getValueDepth(c1));
    EXPECT_TRUE(!acc.isVoxel(c0));
    EXPECT_TRUE(!acc.isVoxel(c1));

    tree.setValue(c0, value);

    EXPECT_TRUE(tree.isValueOn(c0));
    EXPECT_TRUE(!tree.isValueOn(c1));
    ASSERT_DOUBLES_EXACTLY_EQUAL(background, acc.getValue(c1));
    EXPECT_TRUE(!acc.isCached(c1));
    EXPECT_TRUE(!acc.isCached(c0));
    EXPECT_TRUE(acc.isValueOn(c0));
    EXPECT_TRUE(!acc.isValueOn(c1));
    EXPECT_TRUE(acc.probeValue(c0,v));
    ASSERT_DOUBLES_EXACTLY_EQUAL(value, v);
    EXPECT_TRUE(!acc.probeValue(c1,v));
    ASSERT_DOUBLES_EXACTLY_EQUAL(background, v);
    EXPECT_EQ(leafDepth, acc.getValueDepth(c0));
    EXPECT_EQ(-1, acc.getValueDepth(c1));
    EXPECT_TRUE( acc.isVoxel(c0));
    EXPECT_TRUE(!acc.isVoxel(c1));

    ASSERT_DOUBLES_EXACTLY_EQUAL(value, acc.getValue(c0));
    EXPECT_TRUE((acc.numCacheLevels()>0) == acc.isCached(c0));
    ASSERT_DOUBLES_EXACTLY_EQUAL(background, acc.getValue(c1));
    EXPECT_TRUE((acc.numCacheLevels()>0) == acc.isCached(c0));
    EXPECT_TRUE(!acc.isCached(c1));
    EXPECT_TRUE(acc.isValueOn(c0));
    EXPECT_TRUE(!acc.isValueOn(c1));

    tree.setValue(c1, value);

    ASSERT_DOUBLES_EXACTLY_EQUAL(value, acc.getValue(c1));
    EXPECT_TRUE(!acc.isCached(c0));
    EXPECT_TRUE((acc.numCacheLevels()>0) == acc.isCached(c1));
    EXPECT_TRUE(acc.isValueOn(c0));
    EXPECT_TRUE(acc.isValueOn(c1));
    EXPECT_EQ(leafDepth, acc.getValueDepth(c0));
    EXPECT_EQ(leafDepth, acc.getValueDepth(c1));
    EXPECT_TRUE(acc.isVoxel(c0));
    EXPECT_TRUE(acc.isVoxel(c1));

    // The next two lines should not compile, because the acc references a const tree:
    //acc.setValue(c1, value);
    //acc.setValueOff(c1);

    acc.clear();
    EXPECT_TRUE(!acc.isCached(c0));
    EXPECT_TRUE(!acc.isCached(c1));
}

    // cache all node levels
TEST_F(TestValueAccessor, testTree2Accessor)        { accessorTest<ValueAccessor<Tree2Type> >(); }
TEST_F(TestValueAccessor, testTree2AccessorRW)      { accessorTest<ValueAccessorRW<Tree2Type> >(); }
TEST_F(TestValueAccessor, testTree2ConstAccessor)   { constAccessorTest<ValueAccessor<const Tree2Type> >(); }
TEST_F(TestValueAccessor, testTree2ConstAccessorRW) { constAccessorTest<ValueAccessorRW<const Tree2Type> >(); }
    // cache all node levels
TEST_F(TestValueAccessor, testTree3Accessor)        { accessorTest<ValueAccessor<Tree3Type> >(); }
TEST_F(TestValueAccessor, testTree3AccessorRW)      { accessorTest<ValueAccessorRW<Tree3Type> >(); }
TEST_F(TestValueAccessor, testTree3ConstAccessor)   { constAccessorTest<ValueAccessor<const Tree3Type> >(); }
TEST_F(TestValueAccessor, testTree3ConstAccessorRW) { constAccessorTest<ValueAccessorRW<const Tree3Type> >(); }
    // cache all node levels
TEST_F(TestValueAccessor, testTree4Accessor)        { accessorTest<ValueAccessor<Tree4Type> >(); }
TEST_F(TestValueAccessor, testTree4AccessorRW)      { accessorTest<ValueAccessorRW<Tree4Type> >(); }
TEST_F(TestValueAccessor, testTree4ConstAccessor)   { constAccessorTest<ValueAccessor<const Tree4Type> >(); }
TEST_F(TestValueAccessor, testTree4ConstAccessorRW) { constAccessorTest<ValueAccessorRW<const Tree4Type> >(); }
    // cache all node levels
TEST_F(TestValueAccessor, testTree5Accessor)        { accessorTest<ValueAccessor<Tree5Type> >(); }
TEST_F(TestValueAccessor, testTree5AccessorRW)      { accessorTest<ValueAccessorRW<Tree5Type> >(); }
TEST_F(TestValueAccessor, testTree5ConstAccessor)   { constAccessorTest<ValueAccessor<const Tree5Type> >(); }
TEST_F(TestValueAccessor, testTree5ConstAccessorRW) { constAccessorTest<ValueAccessorRW<const Tree5Type> >(); }


TEST_F(TestValueAccessor, testMultithreadedAccessor)
{
#define MAX_COORD 5000

    using AccessorT = laovdb::tree::ValueAccessorRW<Tree4Type>;
    // Substituting the following alias typically results in assertion failures:
    //using AccessorT = laovdb::tree::ValueAccessor<Tree4Type>;

    // Task to perform multiple reads through a shared accessor
    struct ReadTask {
        AccessorT& acc;
        ReadTask(AccessorT& c): acc(c) {}
        void execute()
        {
            for (int i = -MAX_COORD; i < MAX_COORD; ++i) {
                ASSERT_DOUBLES_EXACTLY_EQUAL(double(i), acc.getValue(laovdb::Coord(i)));
            }
        }
    };
    // Task to perform multiple writes through a shared accessor
    struct WriteTask {
        AccessorT& acc;
        WriteTask(AccessorT& c): acc(c) {}
        void execute()
        {
            for (int i = -MAX_COORD; i < MAX_COORD; ++i) {
                float f = acc.getValue(laovdb::Coord(i));
                ASSERT_DOUBLES_EXACTLY_EQUAL(float(i), f);
                acc.setValue(laovdb::Coord(i), float(i));
                ASSERT_DOUBLES_EXACTLY_EQUAL(float(i), acc.getValue(laovdb::Coord(i)));
            }
        }
    };
    // Parent task to spawn multiple parallel read and write tasks
    struct RootTask {
        AccessorT& acc;
        RootTask(AccessorT& c): acc(c) {}
        void execute()
        {
            tbb::task_group tasks;
            for (int i = 0; i < 3; ++i) {
                tasks.run([&] { ReadTask r(acc); r.execute(); });
                tasks.run([&] { WriteTask w(acc); w.execute(); });
            }
            tasks.wait();
        }
    };

    Tree4Type tree(/*background=*/0.5);
    AccessorT acc(tree);
    // Populate the tree.
    for (int i = -MAX_COORD; i < MAX_COORD; ++i) {
        acc.setValue(laovdb::Coord(i), float(i));
    }

    // Run multiple read and write tasks in parallel.
    RootTask root(acc);
    root.execute();

#undef MAX_COORD
}


TEST_F(TestValueAccessor, testAccessorRegistration)
{
    using laovdb::Index;

    const float background = 5.0f, value = -9.345f;
    const laovdb::Coord c0(5, 10, 20);

    laovdb::FloatTree::Ptr tree(new laovdb::FloatTree(background));
    laovdb::tree::ValueAccessor<laovdb::FloatTree> acc(*tree);

    // Set a single leaf voxel via the accessor and verify that
    // the cache is populated.
    acc.setValue(c0, value);
    EXPECT_EQ(Index(1), tree->leafCount());
    EXPECT_EQ(tree->root().getLevel(), tree->nonLeafCount());
    EXPECT_TRUE(acc.getNode<laovdb::FloatTree::LeafNodeType>() != nullptr);

    // Reset the voxel to the background value and verify that no nodes
    // have been deleted and that the cache is still populated.
    tree->setValueOff(c0, background);
    EXPECT_EQ(Index(1), tree->leafCount());
    EXPECT_EQ(tree->root().getLevel(), tree->nonLeafCount());
    EXPECT_TRUE(acc.getNode<laovdb::FloatTree::LeafNodeType>() != nullptr);

    // Prune the tree and verify that only the root node remains and that
    // the cache has been cleared.
    laovdb::tools::prune(*tree);
    //tree->prune();
    EXPECT_EQ(Index(0), tree->leafCount());
    EXPECT_EQ(Index(1), tree->nonLeafCount()); // root node only
    EXPECT_TRUE(acc.getNode<laovdb::FloatTree::LeafNodeType>() == nullptr);

    // Set the leaf voxel again and verify that the cache is repopulated.
    acc.setValue(c0, value);
    EXPECT_EQ(Index(1), tree->leafCount());
    EXPECT_EQ(tree->root().getLevel(), tree->nonLeafCount());
    EXPECT_TRUE(acc.getNode<laovdb::FloatTree::LeafNodeType>() != nullptr);

    // Delete the tree and verify that the cache has been cleared.
    tree.reset();
    EXPECT_TRUE(acc.getTree() == nullptr);
    EXPECT_TRUE(acc.getNode<laovdb::FloatTree::RootNodeType>() == nullptr);
    EXPECT_TRUE(acc.getNode<laovdb::FloatTree::LeafNodeType>() == nullptr);
}


TEST_F(TestValueAccessor, testGetNode)
{
    using LeafT = Tree4Type::LeafNodeType;

    const ValueType background = 5.0f, value = -9.345f;
    const laovdb::Coord c0(5, 10, 20);

    Tree4Type tree(background);
    tree.setValue(c0, value);
    {
        laovdb::tree::ValueAccessor<Tree4Type> acc(tree);
        // Prime the cache.
        acc.getValue(c0);
        // Verify that the cache contains a leaf node.
        LeafT* node = acc.getNode<LeafT>();
        EXPECT_TRUE(node != nullptr);

        // Erase the leaf node from the cache and verify that it is gone.
        acc.eraseNode<LeafT>();
        node = acc.getNode<LeafT>();
        EXPECT_TRUE(node == nullptr);
    }
    {
        // As above, but with a const tree.
        laovdb::tree::ValueAccessor<const Tree4Type> acc(tree);
        acc.getValue(c0);
        const LeafT* node = acc.getNode<const LeafT>();
        EXPECT_TRUE(node != nullptr);

        acc.eraseNode<LeafT>();
        node = acc.getNode<const LeafT>();
        EXPECT_TRUE(node == nullptr);
    }
}
