// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

#include <openvdb/Exceptions.h>

#include <gtest/gtest.h>

class TestExceptions : public ::testing::Test
{
protected:
    template<typename ExceptionT> void testException();
};


template<typename ExceptionT> struct ExceptionTraits
{ static std::string name() { return ""; } };
template<> struct ExceptionTraits<laovdb::ArithmeticError>
{ static std::string name() { return "ArithmeticError"; } };
template<> struct ExceptionTraits<laovdb::IndexError>
{ static std::string name() { return "IndexError"; } };
template<> struct ExceptionTraits<laovdb::IoError>
{ static std::string name() { return "IoError"; } };
template<> struct ExceptionTraits<laovdb::KeyError>
{ static std::string name() { return "KeyError"; } };
template<> struct ExceptionTraits<laovdb::LookupError>
{ static std::string name() { return "LookupError"; } };
template<> struct ExceptionTraits<laovdb::NotImplementedError>
{ static std::string name() { return "NotImplementedError"; } };
template<> struct ExceptionTraits<laovdb::ReferenceError>
{ static std::string name() { return "ReferenceError"; } };
template<> struct ExceptionTraits<laovdb::RuntimeError>
{ static std::string name() { return "RuntimeError"; } };
template<> struct ExceptionTraits<laovdb::TypeError>
{ static std::string name() { return "TypeError"; } };
template<> struct ExceptionTraits<laovdb::ValueError>
{ static std::string name() { return "ValueError"; } };


template<typename ExceptionT>
void
TestExceptions::testException()
{
    std::string ErrorMsg("Error message");

    EXPECT_THROW(OPENVDB_THROW(ExceptionT, ErrorMsg), ExceptionT);

    try {
        OPENVDB_THROW(ExceptionT, ErrorMsg);
    } catch (laovdb::Exception& e) {
        const std::string expectedMsg = ExceptionTraits<ExceptionT>::name() + ": " + ErrorMsg;
        EXPECT_EQ(expectedMsg, std::string(e.what()));
    }
}

TEST_F(TestExceptions, testArithmeticError) { testException<laovdb::ArithmeticError>(); }
TEST_F(TestExceptions, testIndexError) { testException<laovdb::IndexError>(); }
TEST_F(TestExceptions, testIoError) { testException<laovdb::IoError>(); }
TEST_F(TestExceptions, testKeyError) { testException<laovdb::KeyError>(); }
TEST_F(TestExceptions, testLookupError) { testException<laovdb::LookupError>(); }
TEST_F(TestExceptions, testNotImplementedError) { testException<laovdb::NotImplementedError>(); }
TEST_F(TestExceptions, testReferenceError) { testException<laovdb::ReferenceError>(); }
TEST_F(TestExceptions, testRuntimeError) { testException<laovdb::RuntimeError>(); }
TEST_F(TestExceptions, testTypeError) { testException<laovdb::TypeError>(); }
TEST_F(TestExceptions, testValueError) { testException<laovdb::ValueError>(); }
