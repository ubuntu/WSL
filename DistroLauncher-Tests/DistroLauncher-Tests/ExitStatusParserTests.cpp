#include "stdafx.h"
#include "gtest/gtest.h"
#include "exit_status_parser.cpp"

namespace Oobe
{
    namespace internal
    {
        using namespace std::string_literals;

        TEST(ExitStatusParserTests, GoodFileShouldPass)
        {
            std::string buffer{
R"(# That's the shape of the file left by the OOBE (with some errors introduced just for testing).
key1=Value1
action = reboot
  # Comment here
   defaultUid: 1001
)"};
            std::stringstream fakeFile{buffer};
            auto parsed = parseExitStatusFile(fakeFile);
            ASSERT_TRUE(parsed.has_value());
            auto action = std::any_cast<std::string>(parsed.value()["action"]);
            ASSERT_EQ("reboot"s, action);
            auto uid = std::any_cast<unsigned long>(parsed.value()["defaultUid"]);
            ASSERT_EQ(1001, uid);
        }

        TEST(ExitStatusParserTests, EmptyFileShouldFail)
        {
            std::string buffer;
            std::stringstream fakeFile{buffer};
            auto parsed = parseExitStatusFile(fakeFile);
            ASSERT_FALSE(parsed.has_value());
        }

        TEST(ExitStatusParserTests, FileWithOnlyInvalidKeysShouldFail)
        {
            std::string buffer{R"(
key1 = value1
nice: is the life
important: 1002
lost: too_long
)"};
            std::stringstream fakeFile{buffer};
            auto parsed = parseExitStatusFile(fakeFile);
            ASSERT_FALSE(parsed.has_value());
        }

        TEST(ExitStatusParserTests, InvalidValueTypeShouldNotStopTheParsing)
        {
            std::string buffer{R"(
defaultUid=tester
action: reboot
)"};
            std::stringstream fakeFile{buffer};
            auto parsed = parseExitStatusFile(fakeFile);
            ASSERT_TRUE(parsed.has_value());
            auto action = std::any_cast<std::string>(parsed.value()["action"]);
            ASSERT_EQ("reboot"s, action);
        }

        TEST(ExitStatusParserTests, OnlyInvalidValueTypesShouldCauseFailure)
        {
            std::string buffer{R"(defaultUid=tester)"};
            std::stringstream fakeFile{buffer};
            auto parsed = parseExitStatusFile(fakeFile);
            ASSERT_FALSE(parsed.has_value());
        }
    }

}
