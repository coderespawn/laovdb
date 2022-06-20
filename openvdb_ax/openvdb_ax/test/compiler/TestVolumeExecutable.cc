// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

#include <openvdb_ax/compiler/Compiler.h>
#include <openvdb_ax/compiler/VolumeExecutable.h>
#include <openvdb/tools/ValueTransformer.h>

#include <cppunit/extensions/HelperMacros.h>

#include <llvm/ExecutionEngine/ExecutionEngine.h>

class TestVolumeExecutable : public CppUnit::TestCase
{
public:

    CPPUNIT_TEST_SUITE(TestVolumeExecutable);
    CPPUNIT_TEST(testConstructionDestruction);
    CPPUNIT_TEST(testCreateMissingGrids);
    CPPUNIT_TEST(testTreeExecutionLevel);
    CPPUNIT_TEST(testActiveTileStreaming);
    CPPUNIT_TEST(testCompilerCases);
    CPPUNIT_TEST(testExecuteBindings);
    CPPUNIT_TEST_SUITE_END();

    void testConstructionDestruction();
    void testCreateMissingGrids();
    void testTreeExecutionLevel();
    void testActiveTileStreaming();
    void testCompilerCases();
    void testExecuteBindings();
};

CPPUNIT_TEST_SUITE_REGISTRATION(TestVolumeExecutable);

void
TestVolumeExecutable::testConstructionDestruction()
{
    // Test the building and teardown of executable objects. This is primarily to test
    // the destruction of Context and ExecutionEngine LLVM objects. These must be destructed
    // in the correct order (ExecutionEngine, then Context) otherwise LLVM will crash

    // must be initialized, otherwise construction/destruction of llvm objects won't
    // exhibit correct behaviour

    CPPUNIT_ASSERT(laovdb::ax::isInitialized());

    std::shared_ptr<llvm::LLVMContext> C(new llvm::LLVMContext);
    std::unique_ptr<llvm::Module> M(new llvm::Module("test_module", *C));
    std::shared_ptr<const llvm::ExecutionEngine> E(llvm::EngineBuilder(std::move(M))
            .setEngineKind(llvm::EngineKind::JIT)
            .create());

    CPPUNIT_ASSERT(!M);
    CPPUNIT_ASSERT(E);

    std::weak_ptr<llvm::LLVMContext> wC = C;
    std::weak_ptr<const llvm::ExecutionEngine> wE = E;

    // Basic construction

    laovdb::ax::ast::Tree tree;
    laovdb::ax::AttributeRegistry::ConstPtr emptyReg =
        laovdb::ax::AttributeRegistry::create(tree);
    laovdb::ax::VolumeExecutable::Ptr volumeExecutable
        (new laovdb::ax::VolumeExecutable(C, E, emptyReg, nullptr, {}, tree));

    CPPUNIT_ASSERT_EQUAL(2, int(wE.use_count()));
    CPPUNIT_ASSERT_EQUAL(2, int(wC.use_count()));

    C.reset();
    E.reset();

    CPPUNIT_ASSERT_EQUAL(1, int(wE.use_count()));
    CPPUNIT_ASSERT_EQUAL(1, int(wC.use_count()));

    // test destruction

    volumeExecutable.reset();

    CPPUNIT_ASSERT_EQUAL(0, int(wE.use_count()));
    CPPUNIT_ASSERT_EQUAL(0, int(wC.use_count()));
}

void
TestVolumeExecutable::testCreateMissingGrids()
{
    laovdb::ax::Compiler::UniquePtr compiler = laovdb::ax::Compiler::create();
    laovdb::ax::VolumeExecutable::Ptr executable =
        compiler->compile<laovdb::ax::VolumeExecutable>("@a=v@b.x;");
    CPPUNIT_ASSERT(executable);

    executable->setCreateMissing(false);
    executable->setValueIterator(laovdb::ax::VolumeExecutable::IterType::ON);

    laovdb::GridPtrVec grids;
    CPPUNIT_ASSERT_THROW(executable->execute(grids), laovdb::AXExecutionError);
    CPPUNIT_ASSERT(grids.empty());

    executable->setCreateMissing(true);
    executable->setValueIterator(laovdb::ax::VolumeExecutable::IterType::ON);
    executable->execute(grids);

    laovdb::math::Transform::Ptr defaultTransform =
        laovdb::math::Transform::createLinearTransform();

    CPPUNIT_ASSERT_EQUAL(size_t(2), grids.size());
    CPPUNIT_ASSERT(grids[0]->getName() == "b");
    CPPUNIT_ASSERT(grids[0]->isType<laovdb::Vec3fGrid>());
    CPPUNIT_ASSERT(grids[0]->empty());
    CPPUNIT_ASSERT(grids[0]->transform() == *defaultTransform);

    CPPUNIT_ASSERT(grids[1]->getName() == "a");
    CPPUNIT_ASSERT(grids[1]->isType<laovdb::FloatGrid>());
    CPPUNIT_ASSERT(grids[1]->empty());
    CPPUNIT_ASSERT(grids[1]->transform() == *defaultTransform);
}

void
TestVolumeExecutable::testTreeExecutionLevel()
{
    laovdb::ax::CustomData::Ptr data = laovdb::ax::CustomData::create();
    laovdb::FloatMetadata* const meta =
        data->getOrInsertData<laovdb::FloatMetadata>("value");

    laovdb::ax::Compiler::UniquePtr compiler = laovdb::ax::Compiler::create();
    // generate an executable which does not stream active tiles
    laovdb::ax::VolumeExecutable::Ptr executable =
        compiler->compile<laovdb::ax::VolumeExecutable>("f@test = $value;", data);
    CPPUNIT_ASSERT(executable);
    CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::OFF ==
        executable->getActiveTileStreaming());

    using NodeT0 = laovdb::FloatGrid::Accessor::NodeT0;
    using NodeT1 = laovdb::FloatGrid::Accessor::NodeT1;
    using NodeT2 = laovdb::FloatGrid::Accessor::NodeT2;

    laovdb::FloatGrid grid;
    grid.setName("test");
    laovdb::FloatTree& tree = grid.tree();
    tree.addTile(3, laovdb::Coord(0), -1.0f, /*active*/true); // NodeT2 tile
    tree.addTile(2, laovdb::Coord(NodeT2::DIM), -1.0f, /*active*/true); // NodeT1 tile
    tree.addTile(1, laovdb::Coord(NodeT2::DIM+NodeT1::DIM), -1.0f, /*active*/true); // NodeT0 tile
    auto leaf = tree.touchLeaf(laovdb::Coord(NodeT2::DIM + NodeT1::DIM + NodeT0::DIM));
    CPPUNIT_ASSERT(leaf);
    leaf->fill(-1.0f, true);

    const laovdb::FloatTree copy = tree;
    // check config
    auto CHECK_CONFIG = [&]() {
        CPPUNIT_ASSERT_EQUAL(laovdb::Index32(1), tree.leafCount());
        CPPUNIT_ASSERT_EQUAL(laovdb::Index64(3), tree.activeTileCount());
        CPPUNIT_ASSERT_EQUAL(int(laovdb::FloatTree::DEPTH-4), tree.getValueDepth(laovdb::Coord(0)));
        CPPUNIT_ASSERT_EQUAL(int(laovdb::FloatTree::DEPTH-3), tree.getValueDepth(laovdb::Coord(NodeT2::DIM)));
        CPPUNIT_ASSERT_EQUAL(int(laovdb::FloatTree::DEPTH-2), tree.getValueDepth(laovdb::Coord(NodeT2::DIM+NodeT1::DIM)));
        CPPUNIT_ASSERT_EQUAL(leaf, tree.probeLeaf(laovdb::Coord(NodeT2::DIM + NodeT1::DIM + NodeT0::DIM)));
        CPPUNIT_ASSERT_EQUAL(laovdb::Index64(NodeT2::NUM_VOXELS) +
            laovdb::Index64(NodeT1::NUM_VOXELS) +
            laovdb::Index64(NodeT0::NUM_VOXELS) +
            laovdb::Index64(NodeT0::NUM_VOXELS), // leaf
                tree.activeVoxelCount());
        CPPUNIT_ASSERT(copy.hasSameTopology(tree));
    };

    float constant; bool active;

    CHECK_CONFIG();
    CPPUNIT_ASSERT_EQUAL(-1.0f, tree.getValue(laovdb::Coord(0)));
    CPPUNIT_ASSERT_EQUAL(-1.0f, tree.getValue(laovdb::Coord(NodeT2::DIM)));
    CPPUNIT_ASSERT_EQUAL(-1.0f, tree.getValue(laovdb::Coord(NodeT2::DIM+NodeT1::DIM)));
    CPPUNIT_ASSERT(leaf->isConstant(constant, active));
    CPPUNIT_ASSERT_EQUAL(-1.0f, constant);
    CPPUNIT_ASSERT(active);

    laovdb::Index min,max;

    // process default config, all should change
    executable->getTreeExecutionLevel(min,max);
    CPPUNIT_ASSERT_EQUAL(laovdb::Index(0), min);
    CPPUNIT_ASSERT_EQUAL(laovdb::Index(laovdb::FloatTree::DEPTH-1), max);
    meta->setValue(-2.0f);
    executable->execute(grid);
    CHECK_CONFIG();
    CPPUNIT_ASSERT_EQUAL(-2.0f, tree.getValue(laovdb::Coord(0)));
    CPPUNIT_ASSERT_EQUAL(-2.0f, tree.getValue(laovdb::Coord(NodeT2::DIM)));
    CPPUNIT_ASSERT_EQUAL(-2.0f,  tree.getValue(laovdb::Coord(NodeT2::DIM+NodeT1::DIM)));
    CPPUNIT_ASSERT(leaf->isConstant(constant, active));
    CPPUNIT_ASSERT_EQUAL(-2.0f, constant);
    CPPUNIT_ASSERT(active);

    // process level 0, only leaf change
    meta->setValue(1.0f);
    executable->setTreeExecutionLevel(0);
    executable->getTreeExecutionLevel(min,max);
    CPPUNIT_ASSERT_EQUAL(laovdb::Index(0), min);
    CPPUNIT_ASSERT_EQUAL(laovdb::Index(0), max);
    executable->execute(grid);
    CHECK_CONFIG();
    CPPUNIT_ASSERT_EQUAL(-2.0f, tree.getValue(laovdb::Coord(0)));
    CPPUNIT_ASSERT_EQUAL(-2.0f, tree.getValue(laovdb::Coord(NodeT2::DIM)));
    CPPUNIT_ASSERT_EQUAL(-2.0f,  tree.getValue(laovdb::Coord(NodeT2::DIM+NodeT1::DIM)));
    CPPUNIT_ASSERT(leaf->isConstant(constant, active));
    CPPUNIT_ASSERT_EQUAL(1.0f, constant);
    CPPUNIT_ASSERT(active);

    // process level 1
    meta->setValue(3.0f);
    executable->setTreeExecutionLevel(1);
    executable->getTreeExecutionLevel(min,max);
    CPPUNIT_ASSERT_EQUAL(laovdb::Index(1), min);
    CPPUNIT_ASSERT_EQUAL(laovdb::Index(1), max);
    executable->execute(grid);
    CHECK_CONFIG();
    CPPUNIT_ASSERT_EQUAL(-2.0f, tree.getValue(laovdb::Coord(0)));
    CPPUNIT_ASSERT_EQUAL(-2.0f, tree.getValue(laovdb::Coord(NodeT2::DIM)));
    CPPUNIT_ASSERT_EQUAL(3.0f, tree.getValue(laovdb::Coord(NodeT2::DIM+NodeT1::DIM)));
    CPPUNIT_ASSERT(leaf->isConstant(constant, active));
    CPPUNIT_ASSERT_EQUAL(1.0f, constant);
    CPPUNIT_ASSERT(active);

    // process level 2
    meta->setValue(5.0f);
    executable->setTreeExecutionLevel(2);
    executable->getTreeExecutionLevel(min,max);
    CPPUNIT_ASSERT_EQUAL(laovdb::Index(2), min);
    CPPUNIT_ASSERT_EQUAL(laovdb::Index(2), max);
    executable->execute(grid);
    CHECK_CONFIG();
    CPPUNIT_ASSERT_EQUAL(-2.0f, tree.getValue(laovdb::Coord(0)));
    CPPUNIT_ASSERT_EQUAL(5.0f, tree.getValue(laovdb::Coord(NodeT2::DIM)));
    CPPUNIT_ASSERT_EQUAL(3.0f, tree.getValue(laovdb::Coord(NodeT2::DIM+NodeT1::DIM)));
    CPPUNIT_ASSERT(leaf->isConstant(constant, active));
    CPPUNIT_ASSERT_EQUAL(1.0f, constant);
    CPPUNIT_ASSERT(active);

    // process level 3
    meta->setValue(10.0f);
    executable->setTreeExecutionLevel(3);
    executable->getTreeExecutionLevel(min,max);
    CPPUNIT_ASSERT_EQUAL(laovdb::Index(3), min);
    CPPUNIT_ASSERT_EQUAL(laovdb::Index(3), max);
    executable->execute(grid);
    CHECK_CONFIG();
    CPPUNIT_ASSERT_EQUAL(10.0f, tree.getValue(laovdb::Coord(0)));
    CPPUNIT_ASSERT_EQUAL(5.0f, tree.getValue(laovdb::Coord(NodeT2::DIM)));
    CPPUNIT_ASSERT_EQUAL(3.0f, tree.getValue(laovdb::Coord(NodeT2::DIM+NodeT1::DIM)));
    CPPUNIT_ASSERT(leaf->isConstant(constant, active));
    CPPUNIT_ASSERT_EQUAL(1.0f, constant);
    CPPUNIT_ASSERT(active);

    // test higher values throw
    CPPUNIT_ASSERT_THROW(executable->setTreeExecutionLevel(4), laovdb::RuntimeError);

    // test level range 0-1
    meta->setValue(-4.0f);
    executable->setTreeExecutionLevel(0,1);
    executable->getTreeExecutionLevel(min,max);
    CPPUNIT_ASSERT_EQUAL(laovdb::Index(0), min);
    CPPUNIT_ASSERT_EQUAL(laovdb::Index(1), max);
    executable->execute(grid);
    CHECK_CONFIG();
    CPPUNIT_ASSERT_EQUAL(10.0f, tree.getValue(laovdb::Coord(0)));
    CPPUNIT_ASSERT_EQUAL(5.0f, tree.getValue(laovdb::Coord(NodeT2::DIM)));
    CPPUNIT_ASSERT_EQUAL(-4.0f, tree.getValue(laovdb::Coord(NodeT2::DIM+NodeT1::DIM)));
    CPPUNIT_ASSERT(leaf->isConstant(constant, active));
    CPPUNIT_ASSERT_EQUAL(-4.0f, constant);
    CPPUNIT_ASSERT(active);

    // test level range 1-2
    meta->setValue(-6.0f);
    executable->setTreeExecutionLevel(1,2);
    executable->getTreeExecutionLevel(min,max);
    CPPUNIT_ASSERT_EQUAL(laovdb::Index(1), min);
    CPPUNIT_ASSERT_EQUAL(laovdb::Index(2), max);
    executable->execute(grid);
    CHECK_CONFIG();
    CPPUNIT_ASSERT_EQUAL(10.0f, tree.getValue(laovdb::Coord(0)));
    CPPUNIT_ASSERT_EQUAL(-6.0f, tree.getValue(laovdb::Coord(NodeT2::DIM)));
    CPPUNIT_ASSERT_EQUAL(-6.0f, tree.getValue(laovdb::Coord(NodeT2::DIM+NodeT1::DIM)));
    CPPUNIT_ASSERT(leaf->isConstant(constant, active));
    CPPUNIT_ASSERT_EQUAL(-4.0f, constant);
    CPPUNIT_ASSERT(active);

    // test level range 2-3
    meta->setValue(-11.0f);
    executable->setTreeExecutionLevel(2,3);
    executable->getTreeExecutionLevel(min,max);
    CPPUNIT_ASSERT_EQUAL(laovdb::Index(2), min);
    CPPUNIT_ASSERT_EQUAL(laovdb::Index(3), max);
    executable->execute(grid);
    CHECK_CONFIG();
    CPPUNIT_ASSERT_EQUAL(-11.0f, tree.getValue(laovdb::Coord(0)));
    CPPUNIT_ASSERT_EQUAL(-11.0f, tree.getValue(laovdb::Coord(NodeT2::DIM)));
    CPPUNIT_ASSERT_EQUAL(-6.0f, tree.getValue(laovdb::Coord(NodeT2::DIM+NodeT1::DIM)));
    CPPUNIT_ASSERT(leaf->isConstant(constant, active));
    CPPUNIT_ASSERT_EQUAL(-4.0f, constant);
    CPPUNIT_ASSERT(active);

    // test on complete range
    meta->setValue(20.0f);
    executable->setTreeExecutionLevel(0,3);
    executable->getTreeExecutionLevel(min,max);
    CPPUNIT_ASSERT_EQUAL(laovdb::Index(0), min);
    CPPUNIT_ASSERT_EQUAL(laovdb::Index(3), max);
    executable->execute(grid);
    CHECK_CONFIG();
    CPPUNIT_ASSERT_EQUAL(20.0f, tree.getValue(laovdb::Coord(0)));
    CPPUNIT_ASSERT_EQUAL(20.0f, tree.getValue(laovdb::Coord(NodeT2::DIM)));
    CPPUNIT_ASSERT_EQUAL(20.0f, tree.getValue(laovdb::Coord(NodeT2::DIM+NodeT1::DIM)));
    CPPUNIT_ASSERT(leaf->isConstant(constant, active));
    CPPUNIT_ASSERT_EQUAL(20.0f, constant);
    CPPUNIT_ASSERT(active);
}

void
TestVolumeExecutable::testActiveTileStreaming()
{
    using NodeT0 = laovdb::FloatGrid::Accessor::NodeT0;
    using NodeT1 = laovdb::FloatGrid::Accessor::NodeT1;
    using NodeT2 = laovdb::FloatGrid::Accessor::NodeT2;

    //

    laovdb::Index min,max;
    laovdb::ax::VolumeExecutable::Ptr executable;
    laovdb::ax::Compiler::UniquePtr compiler = laovdb::ax::Compiler::create();

    // test no streaming
    {
        laovdb::FloatGrid grid;
        grid.setName("test");
        laovdb::FloatTree& tree = grid.tree();
        tree.addTile(3, laovdb::Coord(0), -1.0f, /*active*/true); // NodeT2 tile
        tree.addTile(2, laovdb::Coord(NodeT2::DIM), -1.0f, /*active*/true); // NodeT1 tile
        tree.addTile(1, laovdb::Coord(NodeT2::DIM+NodeT1::DIM), -1.0f, /*active*/true); // NodeT0 tile
        auto leaf = tree.touchLeaf(laovdb::Coord(NodeT2::DIM + NodeT1::DIM + NodeT0::DIM));
        CPPUNIT_ASSERT(leaf);
        leaf->fill(-1.0f, true);

        executable = compiler->compile<laovdb::ax::VolumeExecutable>("f@test = 2.0f;");
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::OFF ==
            executable->getActiveTileStreaming());
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::OFF ==
            executable->getActiveTileStreaming("test", laovdb::ax::ast::tokens::CoreType::FLOAT));
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::OFF ==
            executable->getActiveTileStreaming("empty", laovdb::ax::ast::tokens::CoreType::FLOAT));

        executable->getTreeExecutionLevel(min,max);
        CPPUNIT_ASSERT_EQUAL(laovdb::Index(0), min);
        CPPUNIT_ASSERT_EQUAL(laovdb::Index(laovdb::FloatTree::DEPTH-1), max);
        executable->execute(grid);

        CPPUNIT_ASSERT_EQUAL(laovdb::Index32(1), tree.leafCount());
        CPPUNIT_ASSERT_EQUAL(laovdb::Index64(3), tree.activeTileCount());
        CPPUNIT_ASSERT_EQUAL(int(laovdb::FloatTree::DEPTH-4), tree.getValueDepth(laovdb::Coord(0)));
        CPPUNIT_ASSERT_EQUAL(int(laovdb::FloatTree::DEPTH-3), tree.getValueDepth(laovdb::Coord(NodeT2::DIM)));
        CPPUNIT_ASSERT_EQUAL(int(laovdb::FloatTree::DEPTH-2), tree.getValueDepth(laovdb::Coord(NodeT2::DIM+NodeT1::DIM)));
        CPPUNIT_ASSERT_EQUAL(int(laovdb::FloatTree::DEPTH-1), tree.getValueDepth(laovdb::Coord(NodeT2::DIM+NodeT1::DIM+NodeT0::DIM)));
        CPPUNIT_ASSERT_EQUAL(laovdb::Index64(NodeT2::NUM_VOXELS) +
            laovdb::Index64(NodeT1::NUM_VOXELS) +
            laovdb::Index64(NodeT0::NUM_VOXELS) +
            laovdb::Index64(NodeT0::NUM_VOXELS), tree.activeVoxelCount());

        CPPUNIT_ASSERT_EQUAL(2.0f, tree.getValue(laovdb::Coord(0)));
        CPPUNIT_ASSERT_EQUAL(2.0f, tree.getValue(laovdb::Coord(NodeT2::DIM)));
        CPPUNIT_ASSERT_EQUAL(2.0f, tree.getValue(laovdb::Coord(NodeT2::DIM+NodeT1::DIM)));
        CPPUNIT_ASSERT_EQUAL(leaf, tree.probeLeaf(laovdb::Coord(NodeT2::DIM + NodeT1::DIM + NodeT0::DIM)));
        float constant; bool active;
        CPPUNIT_ASSERT(leaf->isConstant(constant, active));
        CPPUNIT_ASSERT_EQUAL(2.0f, constant);
        CPPUNIT_ASSERT(active);
    }

    // test getvoxelpws which densifies everything
    {
        laovdb::FloatGrid grid;
        grid.setName("test");
        laovdb::FloatTree& tree = grid.tree();
        tree.addTile(2, laovdb::Coord(0), -1.0f, /*active*/true); // NodeT1 tile
        tree.addTile(1, laovdb::Coord(NodeT1::DIM), -1.0f, /*active*/true); // NodeT0 tile

        executable = compiler->compile<laovdb::ax::VolumeExecutable>("vec3d p = getvoxelpws(); f@test = p.x;");
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming());
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming("test", laovdb::ax::ast::tokens::CoreType::FLOAT));
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming("empty", laovdb::ax::ast::tokens::CoreType::FLOAT));

        executable->getTreeExecutionLevel(min,max);
        CPPUNIT_ASSERT_EQUAL(laovdb::Index(0), min);
        CPPUNIT_ASSERT_EQUAL(laovdb::Index(laovdb::FloatTree::DEPTH-1), max);

        executable->execute(grid);

        const laovdb::Index64 voxels =
            laovdb::Index64(NodeT1::NUM_VOXELS) +
            laovdb::Index64(NodeT0::NUM_VOXELS);

        CPPUNIT_ASSERT_EQUAL(laovdb::Index32(voxels / laovdb::FloatTree::LeafNodeType::NUM_VOXELS), tree.leafCount());
        CPPUNIT_ASSERT_EQUAL(laovdb::Index64(0), tree.activeTileCount());
        CPPUNIT_ASSERT_EQUAL(int(laovdb::FloatTree::DEPTH-1), tree.getValueDepth(laovdb::Coord(0)));
        CPPUNIT_ASSERT_EQUAL(int(laovdb::FloatTree::DEPTH-1), tree.getValueDepth(laovdb::Coord(NodeT1::DIM)));
        CPPUNIT_ASSERT_EQUAL(voxels, tree.activeVoxelCount());

        // test values - this isn't strictly necessary for this group of tests
        // as we really just want to check topology results

        laovdb::tools::foreach(tree.cbeginValueOn(), [&](const auto& it) {
            const laovdb::Coord& coord = it.getCoord();
            const double pos = grid.indexToWorld(coord).x();
            CPPUNIT_ASSERT_EQUAL(*it, float(pos));
        });
    }

    // test spatially varying voxelization
    // @note this tests execution over a NodeT2 which is slow
    {
        laovdb::FloatGrid grid;
        grid.setName("test");
        laovdb::FloatTree& tree = grid.tree();
        tree.addTile(3, laovdb::Coord(0), -1.0f, /*active*/true); // NodeT2 tile
        tree.addTile(2, laovdb::Coord(NodeT2::DIM), -1.0f, /*active*/true); // NodeT1 tile
        tree.addTile(1, laovdb::Coord(NodeT2::DIM+NodeT1::DIM), -1.0f, /*active*/true); // NodeT0 tile

        // sets all x == 0 coordinates to 2.0f. These all reside in the NodeT2 tile
        executable = compiler->compile<laovdb::ax::VolumeExecutable>("int x = getcoordx(); if (x == 0) f@test = 2.0f;");
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming());
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming("test", laovdb::ax::ast::tokens::CoreType::FLOAT));
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming("empty", laovdb::ax::ast::tokens::CoreType::FLOAT));

        executable->getTreeExecutionLevel(min,max);
        CPPUNIT_ASSERT_EQUAL(laovdb::Index(0), min);
        CPPUNIT_ASSERT_EQUAL(laovdb::Index(laovdb::FloatTree::DEPTH-1), max);

        executable->execute(grid);

        const laovdb::Index64 face = NodeT2::DIM * NodeT2::DIM; // face voxel count of NodeT2 x==0
        const laovdb::Index64 leafs = // expected leaf nodes that need to be created
            (face * laovdb::FloatTree::LeafNodeType::DIM) /
            laovdb::FloatTree::LeafNodeType::NUM_VOXELS;

        // number of child nodes in NodeT2;
        const laovdb::Index64 n2ChildAxisCount = NodeT2::DIM / NodeT2::getChildDim();
        const laovdb::Index64 n2ChildCount = n2ChildAxisCount * n2ChildAxisCount * n2ChildAxisCount;

        // number of child nodes in NodeT1;
        const laovdb::Index64 n1ChildAxisCount = NodeT1::DIM / NodeT1::getChildDim();
        const laovdb::Index64 n1ChildCount = n1ChildAxisCount * n1ChildAxisCount * n1ChildAxisCount;

         const laovdb::Index64 tiles = // expected active tiles
            (n2ChildCount -  (n2ChildAxisCount * n2ChildAxisCount)) + // NodeT2 child - a single face
            ((n1ChildCount * (n2ChildAxisCount * n2ChildAxisCount)) - leafs) // NodeT1 face tiles (NodeT0) - leafs
            + 1 /*NodeT1*/ + 1 /*NodeT0*/;

        CPPUNIT_ASSERT_EQUAL(laovdb::Index32(leafs), tree.leafCount());
        CPPUNIT_ASSERT_EQUAL(laovdb::Index64(tiles), tree.activeTileCount());
        CPPUNIT_ASSERT_EQUAL(int(laovdb::FloatTree::DEPTH-3), tree.getValueDepth(laovdb::Coord(NodeT2::DIM)));
        CPPUNIT_ASSERT_EQUAL(int(laovdb::FloatTree::DEPTH-2), tree.getValueDepth(laovdb::Coord(NodeT2::DIM+NodeT1::DIM)));
        CPPUNIT_ASSERT_EQUAL(laovdb::Index64(NodeT2::NUM_VOXELS) +
            laovdb::Index64(NodeT1::NUM_VOXELS) +
            laovdb::Index64(NodeT0::NUM_VOXELS), tree.activeVoxelCount());

        laovdb::tools::foreach(tree.cbeginValueOn(), [&](const auto& it) {
            const laovdb::Coord& coord = it.getCoord();
            if (coord.x() == 0) CPPUNIT_ASSERT_EQUAL(*it,  2.0f);
            else                CPPUNIT_ASSERT_EQUAL(*it, -1.0f);
        });
    }

    // test post pruning - force active streaming with a uniform kernel
    {
        laovdb::FloatGrid grid;
        grid.setName("test");
        laovdb::FloatTree& tree = grid.tree();
        tree.addTile(2, laovdb::Coord(NodeT1::DIM*0, 0, 0), -1.0f, /*active*/true); // NodeT1 tile
        tree.addTile(2, laovdb::Coord(NodeT1::DIM*1, 0, 0), -1.0f, /*active*/true); // NodeT1 tile
        tree.addTile(2, laovdb::Coord(NodeT1::DIM*2, 0, 0), -1.0f, /*active*/true); // NodeT1 tile
        tree.addTile(2, laovdb::Coord(NodeT1::DIM*3, 0, 0), -1.0f, /*active*/true); // NodeT1 tile
        tree.addTile(1, laovdb::Coord(NodeT2::DIM), -1.0f, /*active*/true); // NodeT0 tile

        executable = compiler->compile<laovdb::ax::VolumeExecutable>("f@test = 2.0f;");
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::OFF ==
            executable->getActiveTileStreaming());
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::OFF ==
            executable->getActiveTileStreaming("test", laovdb::ax::ast::tokens::CoreType::FLOAT));
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::OFF ==
            executable->getActiveTileStreaming("empty", laovdb::ax::ast::tokens::CoreType::FLOAT));

        // force stream
        executable->setActiveTileStreaming(laovdb::ax::VolumeExecutable::Streaming::ON);
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming());
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming("test", laovdb::ax::ast::tokens::CoreType::FLOAT));
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming("empty", laovdb::ax::ast::tokens::CoreType::FLOAT));

        executable->getTreeExecutionLevel(min,max);
        CPPUNIT_ASSERT_EQUAL(laovdb::Index(0), min);
        CPPUNIT_ASSERT_EQUAL(laovdb::Index(laovdb::FloatTree::DEPTH-1), max);

        executable->execute(grid);

        CPPUNIT_ASSERT_EQUAL(laovdb::Index32(0), tree.leafCount());
        CPPUNIT_ASSERT_EQUAL(laovdb::Index64(5), tree.activeTileCount());
        CPPUNIT_ASSERT_EQUAL(int(laovdb::FloatTree::DEPTH-3), tree.getValueDepth(laovdb::Coord(NodeT1::DIM*0, 0, 0)));
        CPPUNIT_ASSERT_EQUAL(int(laovdb::FloatTree::DEPTH-3), tree.getValueDepth(laovdb::Coord(NodeT1::DIM*1, 0, 0)));
        CPPUNIT_ASSERT_EQUAL(int(laovdb::FloatTree::DEPTH-3), tree.getValueDepth(laovdb::Coord(NodeT1::DIM*2, 0, 0)));
        CPPUNIT_ASSERT_EQUAL(int(laovdb::FloatTree::DEPTH-3), tree.getValueDepth(laovdb::Coord(NodeT1::DIM*3, 0, 0)));
        CPPUNIT_ASSERT_EQUAL(int(laovdb::FloatTree::DEPTH-2), tree.getValueDepth(laovdb::Coord(NodeT2::DIM)));
        CPPUNIT_ASSERT_EQUAL((laovdb::Index64(NodeT1::NUM_VOXELS)*4) +
            laovdb::Index64(NodeT0::NUM_VOXELS), tree.activeVoxelCount());

        CPPUNIT_ASSERT_EQUAL(2.0f, tree.getValue(laovdb::Coord(NodeT1::DIM*0, 0, 0)));
        CPPUNIT_ASSERT_EQUAL(2.0f, tree.getValue(laovdb::Coord(NodeT1::DIM*1, 0, 0)));
        CPPUNIT_ASSERT_EQUAL(2.0f, tree.getValue(laovdb::Coord(NodeT1::DIM*2, 0, 0)));
        CPPUNIT_ASSERT_EQUAL(2.0f, tree.getValue(laovdb::Coord(NodeT1::DIM*3, 0, 0)));
        CPPUNIT_ASSERT_EQUAL(2.0f, tree.getValue(laovdb::Coord(NodeT2::DIM)));
    }

    // test spatially varying voxelization for bool grids which use specialized implementations
    {
        laovdb::BoolGrid grid;
        grid.setName("test");
        laovdb::BoolTree& tree = grid.tree();
        tree.addTile(2, laovdb::Coord(NodeT1::DIM*0, 0, 0), true, /*active*/true); // NodeT1 tile
        tree.addTile(2, laovdb::Coord(NodeT1::DIM*1, 0, 0), true, /*active*/true); // NodeT1 tile
        tree.addTile(2, laovdb::Coord(NodeT1::DIM*2, 0, 0), true, /*active*/true); // NodeT1 tile
        tree.addTile(2, laovdb::Coord(NodeT1::DIM*3, 0, 0), true, /*active*/true); // NodeT1 tile
        tree.addTile(1, laovdb::Coord(NodeT2::DIM), true, /*active*/true); // NodeT0 tileile

        // sets all x == 0 coordinates to 2.0f. These all reside in the NodeT2 tile
        executable = compiler->compile<laovdb::ax::VolumeExecutable>("int x = getcoordx(); if (x == 0) bool@test = false;");
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming());
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming("test", laovdb::ax::ast::tokens::CoreType::BOOL));
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming("empty", laovdb::ax::ast::tokens::CoreType::FLOAT));
        executable->getTreeExecutionLevel(min,max);
        CPPUNIT_ASSERT_EQUAL(laovdb::Index(0), min);
        CPPUNIT_ASSERT_EQUAL(laovdb::Index(laovdb::BoolTree::DEPTH-1), max);

        executable->execute(grid);

        const laovdb::Index64 face = NodeT1::DIM * NodeT1::DIM; // face voxel count of NodeT2 x==0
        const laovdb::Index64 leafs = // expected leaf nodes that need to be created
            (face * laovdb::BoolTree::LeafNodeType::DIM) /
            laovdb::BoolTree::LeafNodeType::NUM_VOXELS;

        // number of child nodes in NodeT1;
        const laovdb::Index64 n1ChildAxisCount = NodeT1::DIM / NodeT1::getChildDim();
        const laovdb::Index64 n1ChildCount = n1ChildAxisCount * n1ChildAxisCount * n1ChildAxisCount;

         const laovdb::Index64 tiles = // expected active tiles
            (n1ChildCount - leafs) // NodeT1 face tiles (NodeT0) - leafs
            + 3 /*NodeT1*/ + 1 /*NodeT0*/;

        CPPUNIT_ASSERT_EQUAL(laovdb::Index32(leafs), tree.leafCount());
        CPPUNIT_ASSERT_EQUAL(laovdb::Index64(tiles), tree.activeTileCount());
        CPPUNIT_ASSERT_EQUAL(int(laovdb::BoolTree::DEPTH-3), tree.getValueDepth(laovdb::Coord(NodeT1::DIM*1, 0, 0)));
        CPPUNIT_ASSERT_EQUAL(int(laovdb::BoolTree::DEPTH-3), tree.getValueDepth(laovdb::Coord(NodeT1::DIM*2, 0, 0)));
        CPPUNIT_ASSERT_EQUAL(int(laovdb::BoolTree::DEPTH-3), tree.getValueDepth(laovdb::Coord(NodeT1::DIM*3, 0, 0)));
        CPPUNIT_ASSERT_EQUAL(int(laovdb::BoolTree::DEPTH-2), tree.getValueDepth(laovdb::Coord(NodeT2::DIM)));
        CPPUNIT_ASSERT_EQUAL((laovdb::Index64(NodeT1::NUM_VOXELS)*4) +
            laovdb::Index64(NodeT0::NUM_VOXELS), tree.activeVoxelCount());

        laovdb::tools::foreach(tree.cbeginValueOn(), [&](const auto& it) {
            const laovdb::Coord& coord = it.getCoord();
            if (coord.x() == 0) CPPUNIT_ASSERT_EQUAL(*it, false);
            else                CPPUNIT_ASSERT_EQUAL(*it, true);
        });
    }

    // test spatially varying voxelization for string grids which use specialized implementations
    {
OPENVDB_NO_DEPRECATION_WARNING_BEGIN
        laovdb::StringGrid grid;
        grid.setName("test");
        laovdb::StringTree& tree = grid.tree();
        tree.addTile(2, laovdb::Coord(NodeT1::DIM*0, 0, 0), "foo", /*active*/true); // NodeT1 tile
        tree.addTile(2, laovdb::Coord(NodeT1::DIM*1, 0, 0), "foo", /*active*/true); // NodeT1 tile
        tree.addTile(2, laovdb::Coord(NodeT1::DIM*2, 0, 0), "foo", /*active*/true); // NodeT1 tile
        tree.addTile(2, laovdb::Coord(NodeT1::DIM*3, 0, 0), "foo", /*active*/true); // NodeT1 tile
        tree.addTile(1, laovdb::Coord(NodeT2::DIM), "foo", /*active*/true); // NodeT0 tileile

        // sets all x == 0 coordinates to 2.0f. These all reside in the NodeT2 tile
        executable = compiler->compile<laovdb::ax::VolumeExecutable>("int x = getcoordx(); if (x == 0) s@test = \"bar\";");
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming());
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming("test", laovdb::ax::ast::tokens::CoreType::STRING));
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming("empty", laovdb::ax::ast::tokens::CoreType::FLOAT));
        executable->getTreeExecutionLevel(min,max);
        CPPUNIT_ASSERT_EQUAL(laovdb::Index(0), min);
        CPPUNIT_ASSERT_EQUAL(laovdb::Index(laovdb::StringTree::DEPTH-1), max);

        executable->execute(grid);

        const laovdb::Index64 face = NodeT1::DIM * NodeT1::DIM; // face voxel count of NodeT2 x==0
        const laovdb::Index64 leafs = // expected leaf nodes that need to be created
            (face * laovdb::StringTree::LeafNodeType::DIM) /
            laovdb::StringTree::LeafNodeType::NUM_VOXELS;

        // number of child nodes in NodeT1;
        const laovdb::Index64 n1ChildAxisCount = NodeT1::DIM / NodeT1::getChildDim();
        const laovdb::Index64 n1ChildCount = n1ChildAxisCount * n1ChildAxisCount * n1ChildAxisCount;

         const laovdb::Index64 tiles = // expected active tiles
            (n1ChildCount - leafs) // NodeT1 face tiles (NodeT0) - leafs
            + 3 /*NodeT1*/ + 1 /*NodeT0*/;

        CPPUNIT_ASSERT_EQUAL(laovdb::Index32(leafs), tree.leafCount());
        CPPUNIT_ASSERT_EQUAL(laovdb::Index64(tiles), tree.activeTileCount());
        CPPUNIT_ASSERT_EQUAL(int(laovdb::StringTree::DEPTH-3), tree.getValueDepth(laovdb::Coord(NodeT1::DIM*1, 0, 0)));
        CPPUNIT_ASSERT_EQUAL(int(laovdb::StringTree::DEPTH-3), tree.getValueDepth(laovdb::Coord(NodeT1::DIM*2, 0, 0)));
        CPPUNIT_ASSERT_EQUAL(int(laovdb::StringTree::DEPTH-3), tree.getValueDepth(laovdb::Coord(NodeT1::DIM*3, 0, 0)));
        CPPUNIT_ASSERT_EQUAL(int(laovdb::StringTree::DEPTH-2), tree.getValueDepth(laovdb::Coord(NodeT2::DIM)));
        CPPUNIT_ASSERT_EQUAL((laovdb::Index64(NodeT1::NUM_VOXELS)*4) +
            laovdb::Index64(NodeT0::NUM_VOXELS), tree.activeVoxelCount());

        laovdb::tools::foreach(tree.cbeginValueOn(), [&](const auto& it) {
            const laovdb::Coord& coord = it.getCoord();
            if (coord.x() == 0) CPPUNIT_ASSERT_EQUAL(*it, std::string("bar"));
            else                CPPUNIT_ASSERT_EQUAL(*it, std::string("foo"));
        });
OPENVDB_NO_DEPRECATION_WARNING_END
    }

    // test streaming with an OFF iterator (no streaming behaviour) and an ALL iterator (streaming behaviour for ON values only)
    {
        laovdb::FloatGrid grid;
        grid.setName("test");
        laovdb::FloatTree& tree = grid.tree();
        tree.addTile(2, laovdb::Coord(0), -1.0f, /*active*/true); // NodeT1 tile
        tree.addTile(1, laovdb::Coord(NodeT1::DIM), -1.0f, /*active*/true); // NodeT0 tile
        auto leaf = tree.touchLeaf(laovdb::Coord(NodeT1::DIM + NodeT0::DIM));
        CPPUNIT_ASSERT(leaf);
        leaf->fill(-1.0f, true);

        laovdb::FloatTree copy = tree;

        executable = compiler->compile<laovdb::ax::VolumeExecutable>("f@test = float(getcoordx());");
        executable->setValueIterator(laovdb::ax::VolumeExecutable::IterType::OFF);

        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming());
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming("test", laovdb::ax::ast::tokens::CoreType::STRING));
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming("empty", laovdb::ax::ast::tokens::CoreType::FLOAT));
        executable->getTreeExecutionLevel(min,max);
        CPPUNIT_ASSERT_EQUAL(laovdb::Index(0), min);
        CPPUNIT_ASSERT_EQUAL(laovdb::Index(laovdb::FloatTree::DEPTH-1), max);

        executable->execute(grid);

        CPPUNIT_ASSERT_EQUAL(laovdb::Index32(1), tree.leafCount());
        CPPUNIT_ASSERT_EQUAL(laovdb::Index64(2), tree.activeTileCount());
        CPPUNIT_ASSERT(tree.hasSameTopology(copy));
        CPPUNIT_ASSERT_EQUAL(int(laovdb::FloatTree::DEPTH-3), tree.getValueDepth(laovdb::Coord(0)));
        CPPUNIT_ASSERT_EQUAL(int(laovdb::FloatTree::DEPTH-2), tree.getValueDepth(laovdb::Coord(NodeT1::DIM)));
        CPPUNIT_ASSERT_EQUAL(leaf, tree.probeLeaf(laovdb::Coord(NodeT1::DIM + NodeT0::DIM)));
        float constant; bool active;
        CPPUNIT_ASSERT(leaf->isConstant(constant, active));
        CPPUNIT_ASSERT_EQUAL(-1.0f, constant);
        CPPUNIT_ASSERT(active);

        laovdb::tools::foreach(tree.cbeginValueOff(), [&](const auto& it) {
            CPPUNIT_ASSERT_EQUAL(*it, float(it.getCoord().x()));
        });

        laovdb::tools::foreach(tree.cbeginValueOn(), [&](const auto& it) {
            CPPUNIT_ASSERT_EQUAL(*it, -1.0f);
        });

        // test IterType::ALL

        tree.clear();
        tree.addTile(2, laovdb::Coord(0), -1.0f, /*active*/true); // NodeT1 tile
        tree.addTile(1, laovdb::Coord(NodeT1::DIM), -1.0f, /*active*/true); // NodeT0 tile
        leaf = tree.touchLeaf(laovdb::Coord(NodeT1::DIM + NodeT0::DIM));
        CPPUNIT_ASSERT(leaf);
        leaf->fill(-1.0f, /*inactive*/false);

        executable = compiler->compile<laovdb::ax::VolumeExecutable>("f@test = float(getcoordy());");
        executable->setValueIterator(laovdb::ax::VolumeExecutable::IterType::ALL);

        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming());
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming("test", laovdb::ax::ast::tokens::CoreType::STRING));
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming("empty", laovdb::ax::ast::tokens::CoreType::FLOAT));
        executable->getTreeExecutionLevel(min,max);
        CPPUNIT_ASSERT_EQUAL(laovdb::Index(0), min);
        CPPUNIT_ASSERT_EQUAL(laovdb::Index(laovdb::FloatTree::DEPTH-1), max);

        executable->execute(grid);

        const laovdb::Index64 voxels =
            laovdb::Index64(NodeT1::NUM_VOXELS) +
            laovdb::Index64(NodeT0::NUM_VOXELS);

        CPPUNIT_ASSERT_EQUAL(laovdb::Index32(voxels / laovdb::FloatTree::LeafNodeType::NUM_VOXELS) + 1, tree.leafCount());
        CPPUNIT_ASSERT_EQUAL(laovdb::Index64(0), tree.activeTileCount());
        CPPUNIT_ASSERT_EQUAL(voxels, tree.activeVoxelCount());
        CPPUNIT_ASSERT_EQUAL(leaf, tree.probeLeaf(laovdb::Coord(NodeT1::DIM + NodeT0::DIM)));
        CPPUNIT_ASSERT(leaf->getValueMask().isOff());

        laovdb::tools::foreach(tree.cbeginValueAll(), [&](const auto& it) {
            CPPUNIT_ASSERT_EQUAL(*it, float(it.getCoord().y()));
        });
    }

    // test auto streaming
    {
        executable = compiler->compile<laovdb::ax::VolumeExecutable>("f@test = f@other; v@test2 = 1; v@test3 = v@test2;");
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::AUTO ==
            executable->getActiveTileStreaming());
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming("test", laovdb::ax::ast::tokens::CoreType::FLOAT));
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::OFF ==
            executable->getActiveTileStreaming("other", laovdb::ax::ast::tokens::CoreType::FLOAT));
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::OFF ==
            executable->getActiveTileStreaming("test2", laovdb::ax::ast::tokens::CoreType::VEC3F));
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
            executable->getActiveTileStreaming("test3", laovdb::ax::ast::tokens::CoreType::VEC3F));
        //
        CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::AUTO ==
            executable->getActiveTileStreaming("empty", laovdb::ax::ast::tokens::CoreType::FLOAT));
    }

    // test that some particular functions cause streaming to turn on

    executable = compiler->compile<laovdb::ax::VolumeExecutable>("f@test = rand();");
    CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
        executable->getActiveTileStreaming());

    executable = compiler->compile<laovdb::ax::VolumeExecutable>("v@test = getcoord();");
    CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
        executable->getActiveTileStreaming());

    executable = compiler->compile<laovdb::ax::VolumeExecutable>("f@test = getcoordx();");
    CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
        executable->getActiveTileStreaming());

    executable = compiler->compile<laovdb::ax::VolumeExecutable>("f@test = getcoordy();");
    CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
        executable->getActiveTileStreaming());

    executable = compiler->compile<laovdb::ax::VolumeExecutable>("f@test = getcoordz();");
    CPPUNIT_ASSERT(laovdb::ax::VolumeExecutable::Streaming::ON ==
        executable->getActiveTileStreaming());
}


void
TestVolumeExecutable::testCompilerCases()
{
    laovdb::ax::Compiler::UniquePtr compiler = laovdb::ax::Compiler::create();
    CPPUNIT_ASSERT(compiler);
    {
        // with string only
        CPPUNIT_ASSERT(static_cast<bool>(compiler->compile<laovdb::ax::VolumeExecutable>("int i;")));
        CPPUNIT_ASSERT_THROW(compiler->compile<laovdb::ax::VolumeExecutable>("i;"), laovdb::AXCompilerError);
        CPPUNIT_ASSERT_THROW(compiler->compile<laovdb::ax::VolumeExecutable>("i"), laovdb::AXSyntaxError);
        // with AST only
        auto ast = laovdb::ax::ast::parse("i;");
        CPPUNIT_ASSERT_THROW(compiler->compile<laovdb::ax::VolumeExecutable>(*ast), laovdb::AXCompilerError);
    }

    laovdb::ax::Logger logger([](const std::string&) {});

    // using string and logger
    {
        laovdb::ax::VolumeExecutable::Ptr executable =
        compiler->compile<laovdb::ax::VolumeExecutable>("", logger); // empty
        CPPUNIT_ASSERT(executable);
    }
    logger.clear();
    {
        laovdb::ax::VolumeExecutable::Ptr executable =
            compiler->compile<laovdb::ax::VolumeExecutable>("i;", logger); // undeclared variable error
        CPPUNIT_ASSERT(!executable);
        CPPUNIT_ASSERT(logger.hasError());
        logger.clear();
        laovdb::ax::VolumeExecutable::Ptr executable2 =
            compiler->compile<laovdb::ax::VolumeExecutable>("i", logger); // expected ; error (parser)
        CPPUNIT_ASSERT(!executable2);
        CPPUNIT_ASSERT(logger.hasError());
    }
    logger.clear();
    {
        laovdb::ax::VolumeExecutable::Ptr executable =
            compiler->compile<laovdb::ax::VolumeExecutable>("int i = 18446744073709551615;", logger); // warning
        CPPUNIT_ASSERT(executable);
        CPPUNIT_ASSERT(logger.hasWarning());
    }

    // using syntax tree and logger
    logger.clear();
    {
        laovdb::ax::ast::Tree::ConstPtr tree = laovdb::ax::ast::parse("", logger);
        CPPUNIT_ASSERT(tree);
        laovdb::ax::VolumeExecutable::Ptr executable =
            compiler->compile<laovdb::ax::VolumeExecutable>(*tree, logger); // empty
        CPPUNIT_ASSERT(executable);
        logger.clear(); // no tree for line col numbers
        laovdb::ax::VolumeExecutable::Ptr executable2 =
            compiler->compile<laovdb::ax::VolumeExecutable>(*tree, logger); // empty
        CPPUNIT_ASSERT(executable2);
    }
    logger.clear();
    {
        laovdb::ax::ast::Tree::ConstPtr tree = laovdb::ax::ast::parse("i;", logger);
        CPPUNIT_ASSERT(tree);
        laovdb::ax::VolumeExecutable::Ptr executable =
            compiler->compile<laovdb::ax::VolumeExecutable>(*tree, logger); // undeclared variable error
        CPPUNIT_ASSERT(!executable);
        CPPUNIT_ASSERT(logger.hasError());
        logger.clear(); // no tree for line col numbers
        laovdb::ax::VolumeExecutable::Ptr executable2 =
            compiler->compile<laovdb::ax::VolumeExecutable>(*tree, logger); // undeclared variable error
        CPPUNIT_ASSERT(!executable2);
        CPPUNIT_ASSERT(logger.hasError());
    }
    logger.clear();
    {
        laovdb::ax::ast::Tree::ConstPtr tree = laovdb::ax::ast::parse("int i = 18446744073709551615;", logger);
        CPPUNIT_ASSERT(tree);
        laovdb::ax::VolumeExecutable::Ptr executable =
            compiler->compile<laovdb::ax::VolumeExecutable>(*tree, logger); // warning
        CPPUNIT_ASSERT(executable);
        CPPUNIT_ASSERT(logger.hasWarning());
        logger.clear(); // no tree for line col numbers
        laovdb::ax::VolumeExecutable::Ptr executable2 =
            compiler->compile<laovdb::ax::VolumeExecutable>(*tree, logger); // warning
        CPPUNIT_ASSERT(executable2);
        CPPUNIT_ASSERT(logger.hasWarning());
    }
    logger.clear();

    // with copied tree
    {
        laovdb::ax::ast::Tree::ConstPtr tree = laovdb::ax::ast::parse("", logger);
        std::unique_ptr<laovdb::ax::ast::Tree> copy(tree->copy());
        laovdb::ax::VolumeExecutable::Ptr executable =
            compiler->compile<laovdb::ax::VolumeExecutable>(*copy, logger); // empty
        CPPUNIT_ASSERT(executable);
    }
    logger.clear();
    {
        laovdb::ax::ast::Tree::ConstPtr tree = laovdb::ax::ast::parse("i;", logger);
        std::unique_ptr<laovdb::ax::ast::Tree> copy(tree->copy());
        laovdb::ax::VolumeExecutable::Ptr executable =
            compiler->compile<laovdb::ax::VolumeExecutable>(*copy, logger); // undeclared variable error
        CPPUNIT_ASSERT(!executable);
        CPPUNIT_ASSERT(logger.hasError());
    }
    logger.clear();
    {
        laovdb::ax::ast::Tree::ConstPtr tree = laovdb::ax::ast::parse("int i = 18446744073709551615;", logger);
        std::unique_ptr<laovdb::ax::ast::Tree> copy(tree->copy());
        laovdb::ax::VolumeExecutable::Ptr executable =
            compiler->compile<laovdb::ax::VolumeExecutable>(*copy, logger); // warning
        CPPUNIT_ASSERT(executable);
        CPPUNIT_ASSERT(logger.hasWarning());
    }
    logger.clear();
}

void
TestVolumeExecutable::testExecuteBindings()
{
    laovdb::ax::Compiler::UniquePtr compiler = laovdb::ax::Compiler::create();

    laovdb::ax::AttributeBindings bindings;
    bindings.set("b", "a"); // bind b to a

    {
        // multi volumes
        laovdb::FloatGrid::Ptr f1(new laovdb::FloatGrid);
        f1->setName("a");
        f1->tree().setValueOn({0,0,0}, 0.0f);
        std::vector<laovdb::GridBase::Ptr> v { f1 };
        laovdb::ax::VolumeExecutable::Ptr executable =
            compiler->compile<laovdb::ax::VolumeExecutable>("@b = 1.0f;");

        CPPUNIT_ASSERT(executable);
        executable->setAttributeBindings(bindings);
        executable->setCreateMissing(false);
        CPPUNIT_ASSERT_NO_THROW(executable->execute(v));
        CPPUNIT_ASSERT_EQUAL(1.0f, f1->tree().getValue({0,0,0}));
    }

    // binding to existing attribute AND not binding to attribute
    {
        laovdb::FloatGrid::Ptr f1(new laovdb::FloatGrid);
        laovdb::FloatGrid::Ptr f2(new laovdb::FloatGrid);
        f1->setName("a");
        f2->setName("c");
        f1->tree().setValueOn({0,0,0}, 0.0f);
        f2->tree().setValueOn({0,0,0}, 0.0f);
        std::vector<laovdb::GridBase::Ptr> v { f1, f2 };
        laovdb::ax::VolumeExecutable::Ptr executable =
            compiler->compile<laovdb::ax::VolumeExecutable>("@b = 1.0f; @c = 2.0f;");

        CPPUNIT_ASSERT(executable);
        executable->setAttributeBindings(bindings);
        executable->setCreateMissing(false);
        CPPUNIT_ASSERT_NO_THROW(executable->execute(v));
        CPPUNIT_ASSERT_EQUAL(1.0f, f1->tree().getValue({0,0,0}));
        CPPUNIT_ASSERT_EQUAL(2.0f, f2->tree().getValue({0,0,0}));
    }

    // binding to new created attribute AND not binding to new created attribute
    {
        laovdb::FloatGrid::Ptr f2(new laovdb::FloatGrid);
        f2->setName("c");
        f2->tree().setValueOn({0,0,0}, 0.0f);
        std::vector<laovdb::GridBase::Ptr> v { f2 };
        laovdb::ax::VolumeExecutable::Ptr executable =
            compiler->compile<laovdb::ax::VolumeExecutable>("@b = 1.0f; @c = 2.0f;");

        CPPUNIT_ASSERT(executable);
        executable->setAttributeBindings(bindings);
        CPPUNIT_ASSERT_NO_THROW(executable->execute(v));
        CPPUNIT_ASSERT_EQUAL(2.0f, f2->tree().getValue({0,0,0}));
        CPPUNIT_ASSERT_EQUAL(size_t(2), v.size());
    }

    // binding to non existent attribute, not creating, error
    {
        laovdb::FloatGrid::Ptr f2(new laovdb::FloatGrid);
        f2->setName("c");
        f2->tree().setValueOn({0,0,0}, 0.0f);
        std::vector<laovdb::GridBase::Ptr> v { f2 };
        laovdb::ax::VolumeExecutable::Ptr executable =
            compiler->compile<laovdb::ax::VolumeExecutable>("@b = 1.0f; @c = 2.0f;");

        CPPUNIT_ASSERT(executable);
        executable->setAttributeBindings(bindings);
        executable->setCreateMissing(false);
        CPPUNIT_ASSERT_THROW(executable->execute(v), laovdb::AXExecutionError);
    }

    // trying to bind to an attribute and use the original attribute name at same time
    {
        laovdb::FloatGrid::Ptr f2(new laovdb::FloatGrid);
        f2->setName("c");
        f2->tree().setValueOn({0,0,0}, 0.0f);
        std::vector<laovdb::GridBase::Ptr> v { f2 };
        laovdb::ax::VolumeExecutable::Ptr executable =
            compiler->compile<laovdb::ax::VolumeExecutable>("@b = 1.0f; @c = 2.0f;");
        CPPUNIT_ASSERT(executable);
        laovdb::ax::AttributeBindings bindings;
        bindings.set("b","c"); // bind b to c
        CPPUNIT_ASSERT_THROW(executable->setAttributeBindings(bindings), laovdb::AXExecutionError);
   }

    // swap ax and data attributes with bindings
    {
        laovdb::FloatGrid::Ptr f2(new laovdb::FloatGrid);
        f2->setName("c");
        f2->tree().setValueOn({0,0,0}, 0.0f);
        std::vector<laovdb::GridBase::Ptr> v { f2 };
        laovdb::ax::VolumeExecutable::Ptr executable =
            compiler->compile<laovdb::ax::VolumeExecutable>("@b = 1.0f; @c = 2.0f;");
        CPPUNIT_ASSERT(executable);
        laovdb::ax::AttributeBindings bindings;
        bindings.set("b","c"); // bind b to c
        bindings.set("c","b"); // bind c to b

        CPPUNIT_ASSERT_NO_THROW(executable->setAttributeBindings(bindings));
        CPPUNIT_ASSERT_NO_THROW(executable->execute(v));
        CPPUNIT_ASSERT_EQUAL(1.0f, f2->tree().getValue({0,0,0}));
    }

    // test setting bindings and then resetting some of those bindings on the same executable
    {
        laovdb::ax::VolumeExecutable::Ptr executable =
            compiler->compile<laovdb::ax::VolumeExecutable>("@b = 1.0f; @a = 2.0f; @c = 3.0f;");
        CPPUNIT_ASSERT(executable);
        laovdb::ax::AttributeBindings bindings;
        bindings.set("b","a"); // bind b to a
        bindings.set("c","b"); // bind c to b
        bindings.set("a","c"); // bind a to c
        CPPUNIT_ASSERT_NO_THROW(executable->setAttributeBindings(bindings));

        bindings.set("a","b"); // bind a to b
        bindings.set("b","a"); // bind a to b
        CPPUNIT_ASSERT(!bindings.dataNameBoundTo("c")); // c should be unbound
        // check that the set call resets c to c
        CPPUNIT_ASSERT_NO_THROW(executable->setAttributeBindings(bindings));
        const laovdb::ax::AttributeBindings& bindingsOnExecutable = executable->getAttributeBindings();
        CPPUNIT_ASSERT(bindingsOnExecutable.isBoundAXName("c"));
        CPPUNIT_ASSERT_EQUAL(*bindingsOnExecutable.dataNameBoundTo("c"), std::string("c"));
    }
}
