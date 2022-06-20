// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

#include "TestHarness.h"

#include <cppunit/extensions/HelperMacros.h>

using namespace laovdb::points;

class TestKeyword : public unittest_util::AXTestCase
{
public:
    CPPUNIT_TEST_SUITE(TestKeyword);
    CPPUNIT_TEST(testKeywordSimpleReturn);
    CPPUNIT_TEST(testKeywordReturnBranchIf);
    CPPUNIT_TEST(testKeywordReturnBranchLoop);
    CPPUNIT_TEST(testKeywordConditionalReturn);
    CPPUNIT_TEST(testKeywordForLoopKeywords);
    CPPUNIT_TEST(testKeywordWhileLoopKeywords);
    CPPUNIT_TEST(testKeywordDoWhileLoopKeywords);
    CPPUNIT_TEST_SUITE_END();

    void testKeywordSimpleReturn();
    void testKeywordReturnBranchIf();
    void testKeywordReturnBranchLoop();
    void testKeywordConditionalReturn();
    void testKeywordForLoopKeywords();
    void testKeywordWhileLoopKeywords();
    void testKeywordDoWhileLoopKeywords();

};

CPPUNIT_TEST_SUITE_REGISTRATION(TestKeyword);

void
TestKeyword::testKeywordSimpleReturn()
{
    mHarness.addAttribute<int>("return_test0", 0);
    mHarness.executeCode("test/snippets/keyword/simpleReturn");

    AXTESTS_STANDARD_ASSERT();
}

void
TestKeyword::testKeywordReturnBranchIf()
{
    mHarness.addAttribute<int>("return_test1", 1);
    mHarness.executeCode("test/snippets/keyword/returnBranchIf");

    AXTESTS_STANDARD_ASSERT();
}

void
TestKeyword::testKeywordReturnBranchLoop()
{
    mHarness.addAttribute<int>("return_test2", 1);
    mHarness.executeCode("test/snippets/keyword/returnBranchLoop");

    AXTESTS_STANDARD_ASSERT();
}

void
TestKeyword::testKeywordConditionalReturn()
{
    mHarness.addAttribute<int>("return_test3", 3);
    mHarness.executeCode("test/snippets/keyword/conditionalReturn");

    AXTESTS_STANDARD_ASSERT();
}

void
TestKeyword::testKeywordForLoopKeywords()
{
    mHarness.addAttribute<laovdb::Vec3f>("loop_test4", laovdb::Vec3f(1.0,0.0,0.0));
    mHarness.addAttribute<laovdb::Vec3f>("loop_test5", laovdb::Vec3f(1.0,0.0,3.0));
    mHarness.addAttribute<laovdb::Vec3f>("loop_test6", laovdb::Vec3f(1.0,2.0,3.0));
    mHarness.addAttribute<laovdb::Vec3f>("loop_test7", laovdb::Vec3f(1.0,2.0,3.0));
    mHarness.addAttribute<laovdb::Vec3f>("loop_test8", laovdb::Vec3f(1.0,2.0,3.0));
    mHarness.addAttribute<laovdb::math::Mat3s>("loop_test19",
        laovdb::math::Mat3s(1.0,2.0,3.0, 0.0,0.0,0.0, 7.0,8.0,9.0));
    mHarness.addAttribute<laovdb::math::Mat3s>("loop_test20",
        laovdb::math::Mat3s(1.0,0.0,0.0, 0.0,0.0,0.0, 7.0,0.0,0.0));
    mHarness.addAttribute<laovdb::math::Mat3s>("loop_test21",
        laovdb::math::Mat3s(1.0,0.0,3.0, 0.0,0.0,0.0, 7.0,0.0,9.0));
    mHarness.addAttribute<laovdb::Vec3f>("return_test4", laovdb::Vec3f(10,10,10));
    mHarness.executeCode("test/snippets/keyword/forLoopKeywords");

    AXTESTS_STANDARD_ASSERT();
}

void
TestKeyword::testKeywordWhileLoopKeywords()
{
    mHarness.addAttribute<laovdb::Vec3f>("loop_test10", laovdb::Vec3f(1.0,0.0,0.0));
    mHarness.addAttribute<laovdb::Vec3f>("loop_test11", laovdb::Vec3f(0.0,0.0,2.0));
    mHarness.addAttribute<laovdb::Vec3f>("return_test6", laovdb::Vec3f(100,100,100));
    mHarness.executeCode("test/snippets/keyword/whileLoopKeywords");

    AXTESTS_STANDARD_ASSERT();
}


void
TestKeyword::testKeywordDoWhileLoopKeywords()
{
    mHarness.addAttribute<laovdb::Vec3f>("loop_test13", laovdb::Vec3f(1.0,0.0,0.0));
    mHarness.addAttribute<laovdb::Vec3f>("loop_test14", laovdb::Vec3f(0.0,0.0,2.0));
    mHarness.addAttribute<laovdb::Vec3f>("return_test7", laovdb::Vec3f(100,100,100));
    mHarness.executeCode("test/snippets/keyword/doWhileLoopKeywords");

    AXTESTS_STANDARD_ASSERT();
}

