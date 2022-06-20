// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

#include <openvdb/Types.h>
#include <openvdb/openvdb.h>
#include <openvdb/tools/Composite.h>
#include <openvdb/tools/LevelSetSphere.h>
#include <openvdb/util/CpuTimer.h>
#include "util.h" // for unittest_util::makeSphere()

#include <gtest/gtest.h>

#include <algorithm> // for std::max() and std::min()
#include <cmath> // for std::isnan() and std::isinf()
#include <limits> // for std::numeric_limits
#include <sstream>
#include <string>
#include <type_traits>

#define TEST_CSG_VERBOSE 0

#if TEST_CSG_VERBOSE
#include <openvdb/util/CpuTimer.h>
#include <iostream>
#endif

namespace {
using Float433Tree = laovdb::tree::Tree4<float, 4, 3, 3>::Type;
using Float433Grid = laovdb::Grid<Float433Tree>;
}


class TestTreeCombine: public ::testing::Test
{
public:
    void SetUp() override { laovdb::initialize(); Float433Grid::registerGrid(); }
    void TearDown() override { laovdb::uninitialize(); }

protected:
    template<class TreeT, typename TreeComp, typename ValueComp>
    void testComp(const TreeComp&, const ValueComp&);

    template<class TreeT>
    void testCompRepl();

    template<typename TreeT, typename VisitorT>
    typename TreeT::Ptr
    visitCsg(const TreeT& a, const TreeT& b, const TreeT& ref, const VisitorT&);
};


////////////////////////////////////////


namespace {
namespace Local {

template<typename ValueT>
struct OrderDependentCombineOp {
    OrderDependentCombineOp() {}
    void operator()(const ValueT& a, const ValueT& b, ValueT& result) const {
        result = a + ValueT(100) * b; // result is order-dependent on A and B
    }
};

/// Test Tree::combine(), which takes a functor that accepts three arguments
/// (the a, b and result values).
template<typename TreeT>
void combine(TreeT& a, TreeT& b)
{
    a.combine(b, OrderDependentCombineOp<typename TreeT::ValueType>());
}

/// Test Tree::combineExtended(), which takes a functor that accepts a single
/// CombineArgs argument, in which the functor can return a computed active state
/// for the output value.
template<typename TreeT>
void extendedCombine(TreeT& a, TreeT& b)
{
    using ValueT = typename TreeT::ValueType;
    struct ArgsOp {
        static void order(laovdb::CombineArgs<ValueT>& args) {
            // The result is order-dependent on A and B.
            args.setResult(args.a() + ValueT(100) * args.b());
            args.setResultIsActive(args.aIsActive() || args.bIsActive());
        }
    };
    a.combineExtended(b, ArgsOp::order);
}

template<typename TreeT> void compMax(TreeT& a, TreeT& b) { laovdb::tools::compMax(a, b); }
template<typename TreeT> void compMin(TreeT& a, TreeT& b) { laovdb::tools::compMin(a, b); }
template<typename TreeT> void compSum(TreeT& a, TreeT& b) { laovdb::tools::compSum(a, b); }
template<typename TreeT> void compMul(TreeT& a, TreeT& b) { laovdb::tools::compMul(a, b); }\
template<typename TreeT> void compDiv(TreeT& a, TreeT& b) { laovdb::tools::compDiv(a, b); }\

inline float orderf(float a, float b) { return a + 100.0f * b; }
inline float maxf(float a, float b) { return std::max(a, b); }
inline float minf(float a, float b) { return std::min(a, b); }
inline float sumf(float a, float b) { return a + b; }
inline float mulf(float a, float b) { return a * b; }
inline float divf(float a, float b) { return a / b; }

inline laovdb::Vec3f orderv(const laovdb::Vec3f& a, const laovdb::Vec3f& b) { return a+100.0f*b; }
inline laovdb::Vec3f maxv(const laovdb::Vec3f& a, const laovdb::Vec3f& b) {
    const float aMag = a.lengthSqr(), bMag = b.lengthSqr();
    return (aMag > bMag ? a : (bMag > aMag ? b : std::max(a, b)));
}
inline laovdb::Vec3f minv(const laovdb::Vec3f& a, const laovdb::Vec3f& b) {
    const float aMag = a.lengthSqr(), bMag = b.lengthSqr();
    return (aMag < bMag ? a : (bMag < aMag ? b : std::min(a, b)));
}
inline laovdb::Vec3f sumv(const laovdb::Vec3f& a, const laovdb::Vec3f& b) { return a + b; }
inline laovdb::Vec3f mulv(const laovdb::Vec3f& a, const laovdb::Vec3f& b) { return a * b; }
inline laovdb::Vec3f divv(const laovdb::Vec3f& a, const laovdb::Vec3f& b) { return a / b; }

} // namespace Local
} // unnamed namespace


TEST_F(TestTreeCombine, testCombine)
{
    testComp<laovdb::FloatTree>(Local::combine<laovdb::FloatTree>, Local::orderf);
    testComp<laovdb::VectorTree>(Local::combine<laovdb::VectorTree>, Local::orderv);

    testComp<laovdb::FloatTree>(Local::extendedCombine<laovdb::FloatTree>, Local::orderf);
    testComp<laovdb::VectorTree>(Local::extendedCombine<laovdb::VectorTree>, Local::orderv);
}


TEST_F(TestTreeCombine, testCompMax)
{
    testComp<laovdb::FloatTree>(Local::compMax<laovdb::FloatTree>, Local::maxf);
    testComp<laovdb::VectorTree>(Local::compMax<laovdb::VectorTree>, Local::maxv);
}


TEST_F(TestTreeCombine, testCompMin)
{
    testComp<laovdb::FloatTree>(Local::compMin<laovdb::FloatTree>, Local::minf);
    testComp<laovdb::VectorTree>(Local::compMin<laovdb::VectorTree>, Local::minv);
}


TEST_F(TestTreeCombine, testCompSum)
{
    testComp<laovdb::FloatTree>(Local::compSum<laovdb::FloatTree>, Local::sumf);
    testComp<laovdb::VectorTree>(Local::compSum<laovdb::VectorTree>, Local::sumv);
}


TEST_F(TestTreeCombine, testCompProd)
{
    testComp<laovdb::FloatTree>(Local::compMul<laovdb::FloatTree>, Local::mulf);
    testComp<laovdb::VectorTree>(Local::compMul<laovdb::VectorTree>, Local::mulv);
}


TEST_F(TestTreeCombine, testCompDiv)
{
    testComp<laovdb::FloatTree>(Local::compDiv<laovdb::FloatTree>, Local::divf);
    testComp<laovdb::VectorTree>(Local::compDiv<laovdb::VectorTree>, Local::divv);
}


TEST_F(TestTreeCombine, testCompDivByZero)
{
    const laovdb::Coord c0(0), c1(1), c2(2), c3(3), c4(4);

    // Verify that integer-valued grids behave well w.r.t. division by zero.
    {
        const laovdb::Int32 inf = std::numeric_limits<laovdb::Int32>::max();

        laovdb::Int32Tree a(/*background=*/1), b(0);

        a.setValueOn(c0);
        a.setValueOn(c1);
        a.setValueOn(c2, -1);
        a.setValueOn(c3, -1);
        a.setValueOn(c4, 0);
        b.setValueOn(c1);
        b.setValueOn(c3);

        laovdb::tools::compDiv(a, b);

        EXPECT_EQ( inf, a.getValue(c0)); //  1 / 0
        EXPECT_EQ( inf, a.getValue(c1)); //  1 / 0
        EXPECT_EQ(-inf, a.getValue(c2)); // -1 / 0
        EXPECT_EQ(-inf, a.getValue(c3)); // -1 / 0
        EXPECT_EQ(   0, a.getValue(c4)); //  0 / 0
    }
    {
        const laovdb::Index32 zero(0), inf = std::numeric_limits<laovdb::Index32>::max();

        laovdb::UInt32Tree a(/*background=*/1), b(0);

        a.setValueOn(c0);
        a.setValueOn(c1);
        a.setValueOn(c2, zero);
        b.setValueOn(c1);

        laovdb::tools::compDiv(a, b);

        EXPECT_EQ( inf, a.getValue(c0)); //  1 / 0
        EXPECT_EQ( inf, a.getValue(c1)); //  1 / 0
        EXPECT_EQ(zero, a.getValue(c2)); //  0 / 0
    }

    // Verify that non-integer-valued grids don't use integer division semantics.
    {
        laovdb::FloatTree a(/*background=*/1.0), b(0.0);

        a.setValueOn(c0);
        a.setValueOn(c1);
        a.setValueOn(c2, -1.0);
        a.setValueOn(c3, -1.0);
        a.setValueOn(c4, 0.0);
        b.setValueOn(c1);
        b.setValueOn(c3);

        laovdb::tools::compDiv(a, b);

        EXPECT_TRUE(std::isinf(a.getValue(c0))); //  1 / 0
        EXPECT_TRUE(std::isinf(a.getValue(c1))); //  1 / 0
        EXPECT_TRUE(std::isinf(a.getValue(c2))); // -1 / 0
        EXPECT_TRUE(std::isinf(a.getValue(c3))); // -1 / 0
        EXPECT_TRUE(std::isnan(a.getValue(c4))); //  0 / 0
    }
}


TEST_F(TestTreeCombine, testCompReplace)
{
    testCompRepl<laovdb::FloatTree>();
    testCompRepl<laovdb::VectorTree>();
}


template<typename TreeT, typename TreeComp, typename ValueComp>
void
TestTreeCombine::testComp(const TreeComp& comp, const ValueComp& op)
{
    using ValueT = typename TreeT::ValueType;

    const ValueT
        zero = laovdb::zeroVal<ValueT>(),
        minusOne = zero + (-1),
        minusTwo = zero + (-2),
        one = zero + 1,
        three = zero + 3,
        four = zero + 4,
        five = zero + 5;

    {
        TreeT aTree(/*background=*/one);
        aTree.setValueOn(laovdb::Coord(0, 0, 0), three);
        aTree.setValueOn(laovdb::Coord(0, 0, 1), three);
        aTree.setValueOn(laovdb::Coord(0, 0, 2), aTree.background());
        aTree.setValueOn(laovdb::Coord(0, 1, 2), aTree.background());
        aTree.setValueOff(laovdb::Coord(1, 0, 0), three);
        aTree.setValueOff(laovdb::Coord(1, 0, 1), three);

        TreeT bTree(five);
        bTree.setValueOn(laovdb::Coord(0, 0, 0), minusOne);
        bTree.setValueOn(laovdb::Coord(0, 1, 0), four);
        bTree.setValueOn(laovdb::Coord(0, 1, 2), minusTwo);
        bTree.setValueOff(laovdb::Coord(1, 0, 0), minusOne);
        bTree.setValueOff(laovdb::Coord(1, 1, 0), four);

        // Call aTree.compMax(bTree), aTree.compSum(bTree), etc.
        comp(aTree, bTree);

        // a = 3 (On), b = -1 (On)
        EXPECT_EQ(op(three, minusOne), aTree.getValue(laovdb::Coord(0, 0, 0)));

        // a = 3 (On), b = 5 (bg)
        EXPECT_EQ(op(three, five), aTree.getValue(laovdb::Coord(0, 0, 1)));
        EXPECT_TRUE(aTree.isValueOn(laovdb::Coord(0, 0, 1)));

        // a = 1 (On, = bg), b = 5 (bg)
        EXPECT_EQ(op(one, five), aTree.getValue(laovdb::Coord(0, 0, 2)));
        EXPECT_TRUE(aTree.isValueOn(laovdb::Coord(0, 0, 2)));

        // a = 1 (On, = bg), b = -2 (On)
        EXPECT_EQ(op(one, minusTwo), aTree.getValue(laovdb::Coord(0, 1, 2)));
        EXPECT_TRUE(aTree.isValueOn(laovdb::Coord(0, 1, 2)));

        // a = 1 (bg), b = 4 (On)
        EXPECT_EQ(op(one, four), aTree.getValue(laovdb::Coord(0, 1, 0)));
        EXPECT_TRUE(aTree.isValueOn(laovdb::Coord(0, 1, 0)));

        // a = 3 (Off), b = -1 (Off)
        EXPECT_EQ(op(three, minusOne), aTree.getValue(laovdb::Coord(1, 0, 0)));
        EXPECT_TRUE(aTree.isValueOff(laovdb::Coord(1, 0, 0)));

        // a = 3 (Off), b = 5 (bg)
        EXPECT_EQ(op(three, five), aTree.getValue(laovdb::Coord(1, 0, 1)));
        EXPECT_TRUE(aTree.isValueOff(laovdb::Coord(1, 0, 1)));

        // a = 1 (bg), b = 4 (Off)
        EXPECT_EQ(op(one, four), aTree.getValue(laovdb::Coord(1, 1, 0)));
        EXPECT_TRUE(aTree.isValueOff(laovdb::Coord(1, 1, 0)));

        // a = 1 (bg), b = 5 (bg)
        EXPECT_EQ(op(one, five), aTree.getValue(laovdb::Coord(1000, 1, 2)));
        EXPECT_TRUE(aTree.isValueOff(laovdb::Coord(1000, 1, 2)));
    }

    // As above, but combining the A grid into the B grid
    {
        TreeT aTree(/*bg=*/one);
        aTree.setValueOn(laovdb::Coord(0, 0, 0), three);
        aTree.setValueOn(laovdb::Coord(0, 0, 1), three);
        aTree.setValueOn(laovdb::Coord(0, 0, 2), aTree.background());
        aTree.setValueOn(laovdb::Coord(0, 1, 2), aTree.background());
        aTree.setValueOff(laovdb::Coord(1, 0, 0), three);
        aTree.setValueOff(laovdb::Coord(1, 0, 1), three);

        TreeT bTree(five);
        bTree.setValueOn(laovdb::Coord(0, 0, 0), minusOne);
        bTree.setValueOn(laovdb::Coord(0, 1, 0), four);
        bTree.setValueOn(laovdb::Coord(0, 1, 2), minusTwo);
        bTree.setValueOff(laovdb::Coord(1, 0, 0), minusOne);
        bTree.setValueOff(laovdb::Coord(1, 1, 0), four);

        // Call bTree.compMax(aTree), bTree.compSum(aTree), etc.
        comp(bTree, aTree);

        // a = 3 (On), b = -1 (On)
        EXPECT_EQ(op(minusOne, three), bTree.getValue(laovdb::Coord(0, 0, 0)));

        // a = 3 (On), b = 5 (bg)
        EXPECT_EQ(op(five, three), bTree.getValue(laovdb::Coord(0, 0, 1)));
        EXPECT_TRUE(bTree.isValueOn(laovdb::Coord(0, 0, 1)));

        // a = 1 (On, = bg), b = 5 (bg)
        EXPECT_EQ(op(five, one), bTree.getValue(laovdb::Coord(0, 0, 2)));
        EXPECT_TRUE(bTree.isValueOn(laovdb::Coord(0, 0, 2)));

        // a = 1 (On, = bg), b = -2 (On)
        EXPECT_EQ(op(minusTwo, one), bTree.getValue(laovdb::Coord(0, 1, 2)));
        EXPECT_TRUE(bTree.isValueOn(laovdb::Coord(0, 1, 2)));

        // a = 1 (bg), b = 4 (On)
        EXPECT_EQ(op(four, one), bTree.getValue(laovdb::Coord(0, 1, 0)));
        EXPECT_TRUE(bTree.isValueOn(laovdb::Coord(0, 1, 0)));

        // a = 3 (Off), b = -1 (Off)
        EXPECT_EQ(op(minusOne, three), bTree.getValue(laovdb::Coord(1, 0, 0)));
        EXPECT_TRUE(bTree.isValueOff(laovdb::Coord(1, 0, 0)));

        // a = 3 (Off), b = 5 (bg)
        EXPECT_EQ(op(five, three), bTree.getValue(laovdb::Coord(1, 0, 1)));
        EXPECT_TRUE(bTree.isValueOff(laovdb::Coord(1, 0, 1)));

        // a = 1 (bg), b = 4 (Off)
        EXPECT_EQ(op(four, one), bTree.getValue(laovdb::Coord(1, 1, 0)));
        EXPECT_TRUE(bTree.isValueOff(laovdb::Coord(1, 1, 0)));

        // a = 1 (bg), b = 5 (bg)
        EXPECT_EQ(op(five, one), bTree.getValue(laovdb::Coord(1000, 1, 2)));
        EXPECT_TRUE(bTree.isValueOff(laovdb::Coord(1000, 1, 2)));
    }
}


////////////////////////////////////////


TEST_F(TestTreeCombine, testCombine2)
{
    using laovdb::Coord;
    using laovdb::Vec3d;

    struct Local {
        static void floatAverage(const float& a, const float& b, float& result)
            { result = 0.5f * (a + b); }
        static void vec3dAverage(const Vec3d& a, const Vec3d& b, Vec3d& result)
            { result = 0.5 * (a + b); }
        static void vec3dFloatMultiply(const Vec3d& a, const float& b, Vec3d& result)
            { result = a * b; }
        static void vec3dBoolMultiply(const Vec3d& a, const bool& b, Vec3d& result)
            { result = a * b; }
    };

    const Coord c0(0, 0, 0), c1(0, 0, 1), c2(0, 1, 0), c3(1, 0, 0), c4(1000, 1, 2);

    laovdb::FloatTree aFloatTree(/*bg=*/1.0), bFloatTree(5.0), outFloatTree(1.0);
    aFloatTree.setValue(c0, 3.0);
    aFloatTree.setValue(c1, 3.0);
    bFloatTree.setValue(c0, -1.0);
    bFloatTree.setValue(c2, 4.0);
    outFloatTree.combine2(aFloatTree, bFloatTree, Local::floatAverage);

    const float tolerance = 0.0;
    // Average of set value 3 and set value -1
    EXPECT_NEAR(1.0, outFloatTree.getValue(c0), tolerance);
    // Average of set value 3 and bg value 5
    EXPECT_NEAR(4.0, outFloatTree.getValue(c1), tolerance);
    // Average of bg value 1 and set value 4
    EXPECT_NEAR(2.5, outFloatTree.getValue(c2), tolerance);
    // Average of bg value 1 and bg value 5
    EXPECT_TRUE(outFloatTree.isValueOff(c3));
    EXPECT_TRUE(outFloatTree.isValueOff(c4));
    EXPECT_NEAR(3.0, outFloatTree.getValue(c3), tolerance);
    EXPECT_NEAR(3.0, outFloatTree.getValue(c4), tolerance);

    // As above, but combining vector grids:
    const Vec3d zero(0), one(1), two(2), three(3), four(4), five(5);
    laovdb::Vec3DTree aVecTree(/*bg=*/one), bVecTree(five), outVecTree(one);
    aVecTree.setValue(c0, three);
    aVecTree.setValue(c1, three);
    bVecTree.setValue(c0, -1.0 * one);
    bVecTree.setValue(c2, four);
    outVecTree.combine2(aVecTree, bVecTree, Local::vec3dAverage);

    // Average of set value 3 and set value -1
    EXPECT_EQ(one, outVecTree.getValue(c0));
    // Average of set value 3 and bg value 5
    EXPECT_EQ(four, outVecTree.getValue(c1));
    // Average of bg value 1 and set value 4
    EXPECT_EQ(2.5 * one, outVecTree.getValue(c2));
    // Average of bg value 1 and bg value 5
    EXPECT_TRUE(outVecTree.isValueOff(c3));
    EXPECT_TRUE(outVecTree.isValueOff(c4));
    EXPECT_EQ(three, outVecTree.getValue(c3));
    EXPECT_EQ(three, outVecTree.getValue(c4));

    // Multiply the vector tree by the scalar tree.
    {
        laovdb::Vec3DTree vecTree(one);
        vecTree.combine2(outVecTree, outFloatTree, Local::vec3dFloatMultiply);

        // Product of set value (1, 1, 1) and set value 1
        EXPECT_TRUE(vecTree.isValueOn(c0));
        EXPECT_EQ(one, vecTree.getValue(c0));
        // Product of set value (4, 4, 4) and set value 4
        EXPECT_TRUE(vecTree.isValueOn(c1));
        EXPECT_EQ(4 * 4 * one, vecTree.getValue(c1));
        // Product of set value (2.5, 2.5, 2.5) and set value 2.5
        EXPECT_TRUE(vecTree.isValueOn(c2));
        EXPECT_EQ(2.5 * 2.5 * one, vecTree.getValue(c2));
        // Product of bg value (3, 3, 3) and bg value 3
        EXPECT_TRUE(vecTree.isValueOff(c3));
        EXPECT_TRUE(vecTree.isValueOff(c4));
        EXPECT_EQ(3 * 3 * one, vecTree.getValue(c3));
        EXPECT_EQ(3 * 3 * one, vecTree.getValue(c4));
    }

    // Multiply the vector tree by a boolean tree.
    {
        laovdb::BoolTree boolTree(0);
        boolTree.setValue(c0, true);
        boolTree.setValue(c1, false);
        boolTree.setValue(c2, true);

        laovdb::Vec3DTree vecTree(one);
        vecTree.combine2(outVecTree, boolTree, Local::vec3dBoolMultiply);

        // Product of set value (1, 1, 1) and set value 1
        EXPECT_TRUE(vecTree.isValueOn(c0));
        EXPECT_EQ(one, vecTree.getValue(c0));
        // Product of set value (4, 4, 4) and set value 0
        EXPECT_TRUE(vecTree.isValueOn(c1));
        EXPECT_EQ(zero, vecTree.getValue(c1));
        // Product of set value (2.5, 2.5, 2.5) and set value 1
        EXPECT_TRUE(vecTree.isValueOn(c2));
        EXPECT_EQ(2.5 * one, vecTree.getValue(c2));
        // Product of bg value (3, 3, 3) and bg value 0
        EXPECT_TRUE(vecTree.isValueOff(c3));
        EXPECT_TRUE(vecTree.isValueOff(c4));
        EXPECT_EQ(zero, vecTree.getValue(c3));
        EXPECT_EQ(zero, vecTree.getValue(c4));
    }

    // Verify that a vector tree can't be combined into a scalar tree
    // (although the reverse is allowed).
    {
        struct Local2 {
            static void f(const float& a, const Vec3d&, float& result) { result = a; }
        };
        laovdb::FloatTree floatTree(5.0), outTree;
        laovdb::Vec3DTree vecTree(one);
        EXPECT_THROW(outTree.combine2(floatTree, vecTree, Local2::f), laovdb::TypeError);
    }
}


////////////////////////////////////////


TEST_F(TestTreeCombine, testBoolTree)
{
    laovdb::BoolGrid::Ptr sphere = laovdb::BoolGrid::create();

    unittest_util::makeSphere<laovdb::BoolGrid>(/*dim=*/laovdb::Coord(32),
                                                 /*ctr=*/laovdb::Vec3f(0),
                                                 /*radius=*/20.0, *sphere,
                                                 unittest_util::SPHERE_SPARSE_NARROW_BAND);

    laovdb::BoolGrid::Ptr
        aGrid = sphere->copy(),
        bGrid = sphere->copy();

    // CSG operations work only on level sets with a nonzero inside and outside values.
    EXPECT_THROW(laovdb::tools::csgUnion(aGrid->tree(), bGrid->tree()),
        laovdb::ValueError);
    EXPECT_THROW(laovdb::tools::csgIntersection(aGrid->tree(), bGrid->tree()),
        laovdb::ValueError);
    EXPECT_THROW(laovdb::tools::csgDifference(aGrid->tree(), bGrid->tree()),
        laovdb::ValueError);

    laovdb::tools::compSum(aGrid->tree(), bGrid->tree());

    bGrid = sphere->copy();
    laovdb::tools::compMax(aGrid->tree(), bGrid->tree());

    int mismatches = 0;
    laovdb::BoolGrid::ConstAccessor acc = sphere->getConstAccessor();
    for (laovdb::BoolGrid::ValueAllCIter it = aGrid->cbeginValueAll(); it; ++it) {
        if (*it != acc.getValue(it.getCoord())) ++mismatches;
    }
    EXPECT_EQ(0, mismatches);
}


////////////////////////////////////////


template<typename TreeT>
void
TestTreeCombine::testCompRepl()
{
    using ValueT = typename TreeT::ValueType;

    const ValueT
        zero = laovdb::zeroVal<ValueT>(),
        minusOne = zero + (-1),
        one = zero + 1,
        three = zero + 3,
        four = zero + 4,
        five = zero + 5;

    {
        TreeT aTree(/*bg=*/one);
        aTree.setValueOn(laovdb::Coord(0, 0, 0), three);
        aTree.setValueOn(laovdb::Coord(0, 0, 1), three);
        aTree.setValueOn(laovdb::Coord(0, 0, 2), aTree.background());
        aTree.setValueOn(laovdb::Coord(0, 1, 2), aTree.background());
        aTree.setValueOff(laovdb::Coord(1, 0, 0), three);
        aTree.setValueOff(laovdb::Coord(1, 0, 1), three);

        TreeT bTree(five);
        bTree.setValueOn(laovdb::Coord(0, 0, 0), minusOne);
        bTree.setValueOn(laovdb::Coord(0, 1, 0), four);
        bTree.setValueOn(laovdb::Coord(0, 1, 2), minusOne);
        bTree.setValueOff(laovdb::Coord(1, 0, 0), minusOne);
        bTree.setValueOff(laovdb::Coord(1, 1, 0), four);

        // Copy active voxels of bTree into aTree.
        laovdb::tools::compReplace(aTree, bTree);

        // a = 3 (On), b = -1 (On)
        EXPECT_EQ(minusOne, aTree.getValue(laovdb::Coord(0, 0, 0)));

        // a = 3 (On), b = 5 (bg)
        EXPECT_EQ(three, aTree.getValue(laovdb::Coord(0, 0, 1)));
        EXPECT_TRUE(aTree.isValueOn(laovdb::Coord(0, 0, 1)));

        // a = 1 (On, = bg), b = 5 (bg)
        EXPECT_EQ(one, aTree.getValue(laovdb::Coord(0, 0, 2)));
        EXPECT_TRUE(aTree.isValueOn(laovdb::Coord(0, 0, 2)));

        // a = 1 (On, = bg), b = -1 (On)
        EXPECT_EQ(minusOne, aTree.getValue(laovdb::Coord(0, 1, 2)));
        EXPECT_TRUE(aTree.isValueOn(laovdb::Coord(0, 1, 2)));

        // a = 1 (bg), b = 4 (On)
        EXPECT_EQ(four, aTree.getValue(laovdb::Coord(0, 1, 0)));
        EXPECT_TRUE(aTree.isValueOn(laovdb::Coord(0, 1, 0)));

        // a = 3 (Off), b = -1 (Off)
        EXPECT_EQ(three, aTree.getValue(laovdb::Coord(1, 0, 0)));
        EXPECT_TRUE(aTree.isValueOff(laovdb::Coord(1, 0, 0)));

        // a = 3 (Off), b = 5 (bg)
        EXPECT_EQ(three, aTree.getValue(laovdb::Coord(1, 0, 1)));
        EXPECT_TRUE(aTree.isValueOff(laovdb::Coord(1, 0, 1)));

        // a = 1 (bg), b = 4 (Off)
        EXPECT_EQ(one, aTree.getValue(laovdb::Coord(1, 1, 0)));
        EXPECT_TRUE(aTree.isValueOff(laovdb::Coord(1, 1, 0)));

        // a = 1 (bg), b = 5 (bg)
        EXPECT_EQ(one, aTree.getValue(laovdb::Coord(1000, 1, 2)));
        EXPECT_TRUE(aTree.isValueOff(laovdb::Coord(1000, 1, 2)));
    }

    // As above, but combining the A grid into the B grid
    {
        TreeT aTree(/*background=*/one);
        aTree.setValueOn(laovdb::Coord(0, 0, 0), three);
        aTree.setValueOn(laovdb::Coord(0, 0, 1), three);
        aTree.setValueOn(laovdb::Coord(0, 0, 2), aTree.background());
        aTree.setValueOn(laovdb::Coord(0, 1, 2), aTree.background());
        aTree.setValueOff(laovdb::Coord(1, 0, 0), three);
        aTree.setValueOff(laovdb::Coord(1, 0, 1), three);

        TreeT bTree(five);
        bTree.setValueOn(laovdb::Coord(0, 0, 0), minusOne);
        bTree.setValueOn(laovdb::Coord(0, 1, 0), four);
        bTree.setValueOn(laovdb::Coord(0, 1, 2), minusOne);
        bTree.setValueOff(laovdb::Coord(1, 0, 0), minusOne);
        bTree.setValueOff(laovdb::Coord(1, 1, 0), four);

        // Copy active voxels of aTree into bTree.
        laovdb::tools::compReplace(bTree, aTree);

        // a = 3 (On), b = -1 (On)
        EXPECT_EQ(three, bTree.getValue(laovdb::Coord(0, 0, 0)));

        // a = 3 (On), b = 5 (bg)
        EXPECT_EQ(three, bTree.getValue(laovdb::Coord(0, 0, 1)));
        EXPECT_TRUE(bTree.isValueOn(laovdb::Coord(0, 0, 1)));

        // a = 1 (On, = bg), b = 5 (bg)
        EXPECT_EQ(one, bTree.getValue(laovdb::Coord(0, 0, 2)));
        EXPECT_TRUE(bTree.isValueOn(laovdb::Coord(0, 0, 2)));

        // a = 1 (On, = bg), b = -1 (On)
        EXPECT_EQ(one, bTree.getValue(laovdb::Coord(0, 1, 2)));
        EXPECT_TRUE(bTree.isValueOn(laovdb::Coord(0, 1, 2)));

        // a = 1 (bg), b = 4 (On)
        EXPECT_EQ(four, bTree.getValue(laovdb::Coord(0, 1, 0)));
        EXPECT_TRUE(bTree.isValueOn(laovdb::Coord(0, 1, 0)));

        // a = 3 (Off), b = -1 (Off)
        EXPECT_EQ(minusOne, bTree.getValue(laovdb::Coord(1, 0, 0)));
        EXPECT_TRUE(bTree.isValueOff(laovdb::Coord(1, 0, 0)));

        // a = 3 (Off), b = 5 (bg)
        EXPECT_EQ(five, bTree.getValue(laovdb::Coord(1, 0, 1)));
        EXPECT_TRUE(bTree.isValueOff(laovdb::Coord(1, 0, 1)));

        // a = 1 (bg), b = 4 (Off)
        EXPECT_EQ(four, bTree.getValue(laovdb::Coord(1, 1, 0)));
        EXPECT_TRUE(bTree.isValueOff(laovdb::Coord(1, 1, 0)));

        // a = 1 (bg), b = 5 (bg)
        EXPECT_EQ(five, bTree.getValue(laovdb::Coord(1000, 1, 2)));
        EXPECT_TRUE(bTree.isValueOff(laovdb::Coord(1000, 1, 2)));
    }
}


////////////////////////////////////////


#ifdef DWA_OPENVDB
TEST_F(TestTreeCombine, testCsg)
{
    using TreeT = laovdb::FloatTree;
    using TreePtr = TreeT::Ptr;
    using GridT = laovdb::Grid<TreeT>;

    struct Local {
        static TreePtr readFile(const std::string& fname) {
            std::string filename(fname), gridName("LevelSet");
            size_t space = filename.find_last_of(' ');
            if (space != std::string::npos) {
                gridName = filename.substr(space + 1);
                filename.erase(space);
            }

            TreePtr tree;
            laovdb::io::File file(filename);
            file.open();
            if (laovdb::GridBase::Ptr basePtr = file.readGrid(gridName)) {
                if (GridT::Ptr gridPtr = laovdb::gridPtrCast<GridT>(basePtr)) {
                    tree = gridPtr->treePtr();
                }
            }
            file.close();
            return tree;
        }

        //static void writeFile(TreePtr tree, const std::string& filename) {
        //    laovdb::io::File file(filename);
        //    laovdb::GridPtrVec grids;
        //    GridT::Ptr grid = laovdb::createGrid(tree);
        //    grid->setName("LevelSet");
        //    grids.push_back(grid);
        //    file.write(grids);
        //}

        static void visitorUnion(TreeT& a, TreeT& b) { laovdb::tools::csgUnion(a, b); }
        static void visitorIntersect(TreeT& a, TreeT& b) { laovdb::tools::csgIntersection(a, b); }
        static void visitorDiff(TreeT& a, TreeT& b) { laovdb::tools::csgDifference(a, b); }
    };

    TreePtr smallTree1, smallTree2, largeTree1, largeTree2, refTree, outTree;

#if TEST_CSG_VERBOSE
    laovdb::util::CpuTimer timer;
    timer.start();
#endif

    const std::string testDir("/work/rd/fx_tools/vdb_unittest/TestGridCombine::testCsg/");
    smallTree1 = Local::readFile(testDir + "small1.vdb2 LevelSet");
    EXPECT_TRUE(smallTree1.get() != nullptr);
    smallTree2 = Local::readFile(testDir + "small2.vdb2 Cylinder");
    EXPECT_TRUE(smallTree2.get() != nullptr);
    largeTree1 = Local::readFile(testDir + "large1.vdb2 LevelSet");
    EXPECT_TRUE(largeTree1.get() != nullptr);
    largeTree2 = Local::readFile(testDir + "large2.vdb2 LevelSet");
    EXPECT_TRUE(largeTree2.get() != nullptr);

#if TEST_CSG_VERBOSE
    std::cerr << "file read: " << timer.milliseconds() << " msec\n";
#endif

#if TEST_CSG_VERBOSE
    std::cerr << "\n<union>\n";
#endif
    refTree = Local::readFile(testDir + "small_union.vdb2");
    outTree = visitCsg(*smallTree1, *smallTree2, *refTree, Local::visitorUnion);
    //Local::writeFile(outTree, "small_union_out.vdb2");
    refTree = Local::readFile(testDir + "large_union.vdb2");
    outTree = visitCsg(*largeTree1, *largeTree2, *refTree, Local::visitorUnion);
    //Local::writeFile(outTree, "large_union_out.vdb2");

#if TEST_CSG_VERBOSE
    std::cerr << "\n<intersection>\n";
#endif
    refTree = Local::readFile(testDir + "small_intersection.vdb2");
    outTree = visitCsg(*smallTree1, *smallTree2, *refTree, Local::visitorIntersect);
    //Local::writeFile(outTree, "small_intersection_out.vdb2");
    refTree = Local::readFile(testDir + "large_intersection.vdb2");
    outTree = visitCsg(*largeTree1, *largeTree2, *refTree, Local::visitorIntersect);
    //Local::writeFile(outTree, "large_intersection_out.vdb2");

#if TEST_CSG_VERBOSE
    std::cerr << "\n<difference>\n";
#endif
    refTree = Local::readFile(testDir + "small_difference.vdb2");
    outTree = visitCsg(*smallTree1, *smallTree2, *refTree, Local::visitorDiff);
    //Local::writeFile(outTree, "small_difference_out.vdb2");
    refTree = Local::readFile(testDir + "large_difference.vdb2");
    outTree = visitCsg(*largeTree1, *largeTree2, *refTree, Local::visitorDiff);
    //Local::writeFile(outTree, "large_difference_out.vdb2");
}
#endif


template<typename TreeT, typename VisitorT>
typename TreeT::Ptr
TestTreeCombine::visitCsg(const TreeT& aInputTree, const TreeT& bInputTree,
    const TreeT& refTree, const VisitorT& visitor)
{
    using TreePtr = typename TreeT::Ptr;

#if TEST_CSG_VERBOSE
    laovdb::util::CpuTimer timer;
    timer.start();
#endif
    TreePtr aTree(new TreeT(aInputTree));
    TreeT bTree(bInputTree);
#if TEST_CSG_VERBOSE
    std::cerr << "deep copy: " << timer.milliseconds() << " msec\n";
#endif

#if (TEST_CSG_VERBOSE > 1)
    std::cerr << "\nA grid:\n";
    aTree->print(std::cerr, /*verbose=*/3);
    std::cerr << "\nB grid:\n";
    bTree.print(std::cerr, /*verbose=*/3);
    std::cerr << "\nExpected:\n";
    refTree.print(std::cerr, /*verbose=*/3);
    std::cerr << "\n";
#endif

    // Compute the CSG combination of the two grids.
#if TEST_CSG_VERBOSE
    timer.start();
#endif
    visitor(*aTree, bTree);
#if TEST_CSG_VERBOSE
    std::cerr << "combine: " << timer.milliseconds() << " msec\n";
#endif
#if (TEST_CSG_VERBOSE > 1)
    std::cerr << "\nActual:\n";
    aTree->print(std::cerr, /*verbose=*/3);
#endif

    std::ostringstream aInfo, refInfo;
    aTree->print(aInfo, /*verbose=*/2);
    refTree.print(refInfo, /*verbose=*/2);

    EXPECT_EQ(refInfo.str(), aInfo.str());

    EXPECT_TRUE(aTree->hasSameTopology(refTree));

    return aTree;
}


////////////////////////////////////////


TEST_F(TestTreeCombine, testCsgCopy)
{
    const float voxelSize = 0.2f;
    const float radius = 3.0f;
    laovdb::Vec3f center(0.0f, 0.0f, 0.0f);

    laovdb::FloatGrid::Ptr gridA =
        laovdb::tools::createLevelSetSphere<laovdb::FloatGrid>(radius, center, voxelSize);

    laovdb::Coord ijkA = gridA->transform().worldToIndexNodeCentered(center);
    EXPECT_TRUE(gridA->tree().getValue(ijkA) < 0.0f); // center is inside

    center.x() += 3.5f;

    laovdb::FloatGrid::Ptr gridB =
        laovdb::tools::createLevelSetSphere<laovdb::FloatGrid>(radius, center, voxelSize);

    laovdb::Coord ijkB = gridA->transform().worldToIndexNodeCentered(center);
    EXPECT_TRUE(gridB->tree().getValue(ijkB) < 0.0f); // center is inside

    laovdb::FloatGrid::Ptr unionGrid = laovdb::tools::csgUnionCopy(*gridA, *gridB);
    laovdb::FloatGrid::Ptr intersectionGrid = laovdb::tools::csgIntersectionCopy(*gridA, *gridB);
    laovdb::FloatGrid::Ptr differenceGrid = laovdb::tools::csgDifferenceCopy(*gridA, *gridB);

    EXPECT_TRUE(unionGrid.get() != nullptr);
    EXPECT_TRUE(intersectionGrid.get() != nullptr);
    EXPECT_TRUE(differenceGrid.get() != nullptr);

    EXPECT_TRUE(!unionGrid->empty());
    EXPECT_TRUE(!intersectionGrid->empty());
    EXPECT_TRUE(!differenceGrid->empty());

    // test inside / outside sign

    EXPECT_TRUE(unionGrid->tree().getValue(ijkA) < 0.0f);
    EXPECT_TRUE(unionGrid->tree().getValue(ijkB) < 0.0f);

    EXPECT_TRUE(!(intersectionGrid->tree().getValue(ijkA) < 0.0f));
    EXPECT_TRUE(!(intersectionGrid->tree().getValue(ijkB) < 0.0f));

    EXPECT_TRUE(differenceGrid->tree().getValue(ijkA) < 0.0f);
    EXPECT_TRUE(!(differenceGrid->tree().getValue(ijkB) < 0.0f));
}


////////////////////////////////////////

TEST_F(TestTreeCombine, testCompActiveLeafVoxels)
{
    {//replace float tree (default argument)
        laovdb::FloatTree srcTree(0.0f), dstTree(0.0f);

        dstTree.setValue(laovdb::Coord(1,1,1), 1.0f);
        srcTree.setValue(laovdb::Coord(1,1,1), 2.0f);
        srcTree.setValue(laovdb::Coord(8,8,8), 3.0f);

        EXPECT_EQ(1, int(dstTree.leafCount()));
        EXPECT_EQ(2, int(srcTree.leafCount()));
        EXPECT_EQ(1.0f, dstTree.getValue(laovdb::Coord(1, 1, 1)));
        EXPECT_TRUE(dstTree.isValueOn(laovdb::Coord(1, 1, 1)));
        EXPECT_EQ(0.0f, dstTree.getValue(laovdb::Coord(8, 8, 8)));
        EXPECT_TRUE(!dstTree.isValueOn(laovdb::Coord(8, 8, 8)));

        laovdb::tools::compActiveLeafVoxels(srcTree, dstTree);

        EXPECT_EQ(2, int(dstTree.leafCount()));
        EXPECT_EQ(0, int(srcTree.leafCount()));
        EXPECT_EQ(2.0f, dstTree.getValue(laovdb::Coord(1, 1, 1)));
        EXPECT_TRUE(dstTree.isValueOn(laovdb::Coord(1, 1, 1)));
        EXPECT_EQ(3.0f, dstTree.getValue(laovdb::Coord(8, 8, 8)));
        EXPECT_TRUE(dstTree.isValueOn(laovdb::Coord(8, 8, 8)));
    }
    {//replace float tree (lambda expression)
        laovdb::FloatTree srcTree(0.0f), dstTree(0.0f);

        dstTree.setValue(laovdb::Coord(1,1,1), 1.0f);
        srcTree.setValue(laovdb::Coord(1,1,1), 2.0f);
        srcTree.setValue(laovdb::Coord(8,8,8), 3.0f);

        EXPECT_EQ(1, int(dstTree.leafCount()));
        EXPECT_EQ(2, int(srcTree.leafCount()));
        EXPECT_EQ(1.0f, dstTree.getValue(laovdb::Coord(1, 1, 1)));
        EXPECT_TRUE(dstTree.isValueOn(laovdb::Coord(1, 1, 1)));
        EXPECT_EQ(0.0f, dstTree.getValue(laovdb::Coord(8, 8, 8)));
        EXPECT_TRUE(!dstTree.isValueOn(laovdb::Coord(8, 8, 8)));

        laovdb::tools::compActiveLeafVoxels(srcTree, dstTree, [](float &d, float s){d=s;});

        EXPECT_EQ(2, int(dstTree.leafCount()));
        EXPECT_EQ(0, int(srcTree.leafCount()));
        EXPECT_EQ(2.0f, dstTree.getValue(laovdb::Coord(1, 1, 1)));
        EXPECT_TRUE(dstTree.isValueOn(laovdb::Coord(1, 1, 1)));
        EXPECT_EQ(3.0f, dstTree.getValue(laovdb::Coord(8, 8, 8)));
        EXPECT_TRUE(dstTree.isValueOn(laovdb::Coord(8, 8, 8)));
    }
    {//add float tree
        laovdb::FloatTree srcTree(0.0f), dstTree(0.0f);

        dstTree.setValue(laovdb::Coord(1,1,1), 1.0f);
        srcTree.setValue(laovdb::Coord(1,1,1), 2.0f);
        srcTree.setValue(laovdb::Coord(8,8,8), 3.0f);

        EXPECT_EQ(1, int(dstTree.leafCount()));
        EXPECT_EQ(2, int(srcTree.leafCount()));
        EXPECT_EQ(1.0f, dstTree.getValue(laovdb::Coord(1, 1, 1)));
        EXPECT_TRUE(dstTree.isValueOn(laovdb::Coord(1, 1, 1)));
        EXPECT_EQ(0.0f, dstTree.getValue(laovdb::Coord(8, 8, 8)));
        EXPECT_TRUE(!dstTree.isValueOn(laovdb::Coord(8, 8, 8)));

        laovdb::tools::compActiveLeafVoxels(srcTree, dstTree, [](float &d, float s){d+=s;});

        EXPECT_EQ(2, int(dstTree.leafCount()));
        EXPECT_EQ(0, int(srcTree.leafCount()));
        EXPECT_EQ(3.0f, dstTree.getValue(laovdb::Coord(1, 1, 1)));
        EXPECT_TRUE(dstTree.isValueOn(laovdb::Coord(1, 1, 1)));
        EXPECT_EQ(3.0f, dstTree.getValue(laovdb::Coord(8, 8, 8)));
        EXPECT_TRUE(dstTree.isValueOn(laovdb::Coord(8, 8, 8)));
    }
    {
        using BufferT = laovdb::FloatTree::LeafNodeType::Buffer;
        EXPECT_TRUE((std::is_same<BufferT::ValueType, BufferT::StorageType>::value));
    }
    {
        using BufferT = laovdb::Vec3fTree::LeafNodeType::Buffer;
        EXPECT_TRUE((std::is_same<BufferT::ValueType, BufferT::StorageType>::value));
    }
    {
        using BufferT = laovdb::BoolTree::LeafNodeType::Buffer;
        EXPECT_TRUE(!(std::is_same<BufferT::ValueType, BufferT::StorageType>::value));
    }
    {
        using BufferT = laovdb::MaskTree::LeafNodeType::Buffer;
        EXPECT_TRUE(!(std::is_same<BufferT::ValueType, BufferT::StorageType>::value));
    }
    {//replace bool tree
        laovdb::BoolTree srcTree(false), dstTree(false);

        dstTree.setValue(laovdb::Coord(1,1,1), true);
        srcTree.setValue(laovdb::Coord(1,1,1), false);
        srcTree.setValue(laovdb::Coord(8,8,8), true);
        //(9,8,8) is inactive but true so it should have no effect
        srcTree.setValueOnly(laovdb::Coord(9,8,8), true);

        EXPECT_EQ(1, int(dstTree.leafCount()));
        EXPECT_EQ(2, int(srcTree.leafCount()));
        EXPECT_EQ(true, dstTree.getValue(laovdb::Coord(1, 1, 1)));
        EXPECT_TRUE(dstTree.isValueOn(laovdb::Coord(1, 1, 1)));
        EXPECT_EQ(false, dstTree.getValue(laovdb::Coord(8, 8, 8)));
        EXPECT_TRUE(!dstTree.isValueOn(laovdb::Coord(8, 8, 8)));
        EXPECT_EQ(true, srcTree.getValue(laovdb::Coord(9, 8, 8)));
        EXPECT_TRUE(!srcTree.isValueOn(laovdb::Coord(9, 8, 8)));

        using Word = laovdb::BoolTree::LeafNodeType::Buffer::WordType;
        laovdb::tools::compActiveLeafVoxels(srcTree, dstTree, [](Word &d, Word s){d=s;});

        EXPECT_EQ(2, int(dstTree.leafCount()));
        EXPECT_EQ(0, int(srcTree.leafCount()));
        EXPECT_EQ(false, dstTree.getValue(laovdb::Coord(1, 1, 1)));
        EXPECT_TRUE(dstTree.isValueOn(laovdb::Coord(1, 1, 1)));
        EXPECT_EQ(true, dstTree.getValue(laovdb::Coord(8, 8, 8)));
        EXPECT_TRUE(dstTree.isValueOn(laovdb::Coord(8, 8, 8)));
    }
    {// mask tree
        laovdb::MaskTree srcTree(false), dstTree(false);

        dstTree.setValueOn(laovdb::Coord(1,1,1));
        srcTree.setValueOn(laovdb::Coord(1,1,1));
        srcTree.setValueOn(laovdb::Coord(8,8,8));

        EXPECT_EQ(1, int(dstTree.leafCount()));
        EXPECT_EQ(2, int(srcTree.leafCount()));
        EXPECT_EQ(true, dstTree.getValue(laovdb::Coord(1, 1, 1)));
        EXPECT_TRUE(dstTree.isValueOn(laovdb::Coord(1, 1, 1)));
        EXPECT_EQ(false, dstTree.getValue(laovdb::Coord(8, 8, 8)));
        EXPECT_TRUE(!dstTree.isValueOn(laovdb::Coord(8, 8, 8)));

        laovdb::tools::compActiveLeafVoxels(srcTree, dstTree);

        EXPECT_EQ(2, int(dstTree.leafCount()));
        EXPECT_EQ(0, int(srcTree.leafCount()));
        EXPECT_EQ(true, dstTree.getValue(laovdb::Coord(1, 1, 1)));
        EXPECT_TRUE(dstTree.isValueOn(laovdb::Coord(1, 1, 1)));
        EXPECT_EQ(true, dstTree.getValue(laovdb::Coord(8, 8, 8)));
        EXPECT_TRUE(dstTree.isValueOn(laovdb::Coord(8, 8, 8)));
    }
}


////////////////////////////////////////

