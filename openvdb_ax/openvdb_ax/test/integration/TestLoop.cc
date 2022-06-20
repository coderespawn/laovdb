// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

#include "TestHarness.h"

#include <cppunit/extensions/HelperMacros.h>

using namespace laovdb::points;

class TestLoop : public unittest_util::AXTestCase
{
public:
    CPPUNIT_TEST_SUITE(TestLoop);
    CPPUNIT_TEST(testLoopForLoop);
    CPPUNIT_TEST(testLoopWhileLoop);
    CPPUNIT_TEST(testLoopDoWhileLoop);
    CPPUNIT_TEST(testLoopOverflow);
    CPPUNIT_TEST(testLoopErrors);
    CPPUNIT_TEST_SUITE_END();

    void testLoopForLoop();
    void testLoopWhileLoop();
    void testLoopDoWhileLoop();
    void testLoopOverflow();
    void testLoopErrors();
};

CPPUNIT_TEST_SUITE_REGISTRATION(TestLoop);

void
TestLoop::testLoopForLoop()
{
    mHarness.addAttribute<laovdb::Vec3f>("loop_test1",  laovdb::Vec3f(1.0,2.0,3.0));
    mHarness.addAttribute<laovdb::Vec3f>("loop_test2",  laovdb::Vec3f(1.0,2.0,3.0));
    mHarness.addAttribute<laovdb::Vec3f>("loop_test3",  laovdb::Vec3f(1.0,2.0,3.0));
    mHarness.addAttribute<laovdb::Vec3f>("loop_test15", laovdb::Vec3f(0.0,0.0,0.0));
    mHarness.addAttribute<laovdb::math::Mat3s>("loop_test18",
        laovdb::math::Mat3s(1.0,2.0,3.0, 4.0,5.0,6.0, 7.0,8.0,9.0));
    mHarness.addAttribute<int32_t>("loop_test22", 3);
    mHarness.addAttribute<int32_t>("loop_test23", 4);
    mHarness.addAttribute<int32_t>("loop_test25", 1);
    mHarness.addAttribute<int32_t>("loop_test27", 14);
    mHarness.addAttribute<int32_t>("loop_test30", 19);
    mHarness.executeCode("test/snippets/loop/forLoop");

    AXTESTS_STANDARD_ASSERT();
}

void
TestLoop::testLoopWhileLoop()
{
    mHarness.addAttribute<laovdb::Vec3f>("loop_test9",  laovdb::Vec3f(1.0,2.0,3.0));
    mHarness.addAttribute<laovdb::Vec3f>("loop_test16", laovdb::Vec3f(0.0,0.0,0.0));
    mHarness.addAttribute<laovdb::Vec3f>("loop_test28", laovdb::Vec3f(0.0,0.0,0.0));
    mHarness.addAttribute<int32_t>("loop_test31", 2);
    mHarness.executeCode("test/snippets/loop/whileLoop");

    AXTESTS_STANDARD_ASSERT();
}

void
TestLoop::testLoopDoWhileLoop()
{
    mHarness.addAttribute<laovdb::Vec3f>("loop_test12", laovdb::Vec3f(1.0,2.0,3.0));
    mHarness.addAttribute<laovdb::Vec3f>("loop_test17", laovdb::Vec3f(1.0,0.0,0.0));
    mHarness.addAttribute<laovdb::Vec3f>("loop_test29", laovdb::Vec3f(1.0,0.0,0.0));
    mHarness.addAttribute<int32_t>("loop_test32", 2);
    mHarness.executeCode("test/snippets/loop/doWhileLoop");

    AXTESTS_STANDARD_ASSERT();
}

void
TestLoop::testLoopOverflow()
{
    // Disable all optimizations to force the loop to not remove the interior
    // allocation. The loop should generate its allocas in the function prologue
    // to avoid stack overflow
    laovdb::ax::CompilerOptions opts;
    opts.mOptLevel = laovdb::ax::CompilerOptions::OptLevel::NONE;
    mHarness.mOpts = opts;
    mHarness.executeCode("test/snippets/loop/loopOverflow");
}

void
TestLoop::testLoopErrors()
{
    const bool success = mHarness.executeCode("test/snippets/loop/loopErrors");
    CPPUNIT_ASSERT(!success);
}

