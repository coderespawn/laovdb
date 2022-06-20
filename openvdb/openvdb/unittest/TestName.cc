// Copyright Contributors to the OpenVDB Project
// SPDX-License-Identifier: MPL-2.0

#include <openvdb/Exceptions.h>
#include <openvdb/util/Name.h>

#include <gtest/gtest.h>


class TestName : public ::testing::Test
{
};


TEST_F(TestName, test)
{
    using namespace laovdb;

    Name name;
    Name name2("something");
    Name name3 = std::string("something2");
    name = "something";

    EXPECT_TRUE(name == name2);
    EXPECT_TRUE(name != name3);
    EXPECT_TRUE(name != Name("testing"));
    EXPECT_TRUE(name == Name("something"));
}

TEST_F(TestName, testIO)
{
    using namespace laovdb;

    Name name("some name that i made up");

    std::ostringstream ostr(std::ios_base::binary);

    laovdb::writeString(ostr, name);

    name = "some other name";

    EXPECT_TRUE(name == Name("some other name"));

    std::istringstream istr(ostr.str(), std::ios_base::binary);

    name = laovdb::readString(istr);

    EXPECT_TRUE(name == Name("some name that i made up"));
}

TEST_F(TestName, testMultipleIO)
{
    using namespace laovdb;

    Name name("some name that i made up");
    Name name2("something else");

    std::ostringstream ostr(std::ios_base::binary);

    laovdb::writeString(ostr, name);
    laovdb::writeString(ostr, name2);

    std::istringstream istr(ostr.str(), std::ios_base::binary);

    Name n = laovdb::readString(istr), n2 = laovdb::readString(istr);

    EXPECT_TRUE(name == n);
    EXPECT_TRUE(name2 == n2);
}
