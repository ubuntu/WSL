#include "stdafx.h"
#include "gtest/gtest.h"

#include "ini_find_value.cpp"

// The assertions below derive from real life experiments in WSL.
namespace Oobe
{
    namespace internal
    {

        TEST(IniFindValueTests, GoodFileShouldPass)
        {
            std::wstring buffer{LR"(# This mimics the syntax of /etc/wsl.conf file with comments.
[user]
default = root
[boot]
command = /usr/libexec/wsl-systemd
)"};
            std::wstringstream fakeFile{buffer};
            ASSERT_TRUE(ini_find_value(fakeFile, L"boot", L"command", L"/usr/libexec/wsl-systemd"));
            ASSERT_TRUE(ini_find_value(fakeFile, L"user", L"default", L"root"));
            ASSERT_FALSE(ini_find_value(fakeFile, L"automount", L"enabled", L"true"));
        }

        TEST(IniFindValueTests, ExtraSpacesAreFine)
        {
            std::wstring buffer{LR"(# This mimics the syntax of /etc/wsl.conf file with comments.
  [user]
          default     =    root
  [boot]
          command  =  /usr/libexec/wsl-systemd
)"};
            std::wstringstream fakeFile{buffer};
            ASSERT_TRUE(ini_find_value(fakeFile, L"boot", L"command", L"/usr/libexec/wsl-systemd"));
            ASSERT_TRUE(ini_find_value(fakeFile, L"user", L"default", L"root"));
        }

        TEST(IniFindValueTests, LessSpacesAreFine)
        {
            std::wstring buffer{LR"(# This mimics the syntax of /etc/wsl.conf file with comments.
[user]
default=root
[boot]
command=/usr/libexec/wsl-systemd
)"};
            std::wstringstream fakeFile{buffer};
            ASSERT_TRUE(ini_find_value(fakeFile, L"boot", L"command", L"/usr/libexec/wsl-systemd"));
            ASSERT_TRUE(ini_find_value(fakeFile, L"user", L"default", L"root"));
        }

        TEST(IniFindValueTests, SurroundingSpacesInsideSectionBreaks)
        {
            std::wstring buffer{LR"(# This mimics the syntax of /etc/wsl.conf file with comments.
  [user ]
 default  =  root 
  [ boot]
 command  =  /usr/libexec/wsl-systemd
)"};
            std::wstringstream fakeFile{buffer};
            ASSERT_FALSE(ini_find_value(fakeFile, L"boot", L"command", L"/usr/libexec/wsl-systemd"));
            ASSERT_FALSE(ini_find_value(fakeFile, L"user", L"default", L"root"));
        }

        TEST(IniFindValueTests, SemicolonCommentsBreaksWSL)
        {
            std::wstring buffer{LR"(# This mimics the syntax of /etc/wsl.conf file with comments.
; Some INI dialects accept semicolon comments. WSL breaks on this and ignores the rest of the file.
[user]
default = root
[boot]
command = /usr/libexec/wsl-systemd
)"};
            std::wstringstream fakeFile{buffer};
            ASSERT_FALSE(ini_find_value(fakeFile, L"boot", L"command", L"/usr/libexec/wsl-systemd"));
            ASSERT_FALSE(ini_find_value(fakeFile, L"user", L"default", L"root"));
        }

        TEST(IniFindValueTests, CommentedLineCommandMustResultFalse)
        {
            std::wstring buffer{LR"(# This mimics the syntax of /etc/wsl.conf file with comments.
[user]
# default = root
[boot]
# command = /usr/libexec/wsl-systemd
# [automount]
enable = true
)"};
            std::wstringstream fakeFile{buffer};
            ASSERT_FALSE(ini_find_value(fakeFile, L"boot", L"command", L"/usr/libexec/wsl-systemd"));
            ASSERT_FALSE(ini_find_value(fakeFile, L"user", L"default", L"root"));
            ASSERT_FALSE(ini_find_value(fakeFile, L"automount", L"enabled", L"true"));
        }
        TEST(IniFindValueTests, IllFormedLinesStopParsing)
        {
            std::wstring buffer{LR"([automount]

enabled = true
mountFsTab = true

[boot]
command = /usr/libexec/wsl-systemd
[user]
name

# The following configuration will never be applied because the syntax is broken in the line above. But the previous were.
default = root
)"};
            std::wstringstream fakeFile{buffer};
            ASSERT_TRUE(ini_find_value(fakeFile, L"boot", L"command", L"/usr/libexec/wsl-systemd"));
            ASSERT_TRUE(ini_find_value(fakeFile, L"automount", L"enabled", L"true"));
            ASSERT_TRUE(ini_find_value(fakeFile, L"automount", L"mountFsTab", L"true"));
            ASSERT_FALSE(ini_find_value(fakeFile, L"user", L"default", L"root"));
        }
        TEST(IniFindValueTests, IllFormedSectionAlsoStopParsing)
        {
            std::wstring buffer{LR"([automount]
enabled = true
mountFsTab = true
[network
[boot]
command = /usr/libexec/wsl-systemd
)"};
            std::wstringstream fakeFile{buffer};
            ASSERT_TRUE(ini_find_value(fakeFile, L"automount", L"enabled", L"true"));
            ASSERT_TRUE(ini_find_value(fakeFile, L"automount", L"mountFsTab", L"true"));
            ASSERT_FALSE(ini_find_value(fakeFile, L"boot", L"command", L"/usr/libexec/wsl-systemd"));
        }
        TEST(IniFindValueTests, SectionsCanBeExtended)
        {
            std::wstring buffer{LR"([automount]
enabled = true
[boot]
command = /usr/libexec/wsl-systemd
[automount]
mountFsTab = true
)"};
            std::wstringstream fakeFile{buffer};
            ASSERT_TRUE(ini_find_value(fakeFile, L"automount", L"enabled", L"true"));
            ASSERT_TRUE(ini_find_value(fakeFile, L"automount", L"mountFsTab", L"true"));
        }
        TEST(IniFindValueTests, EmptySectionsWontAffectOthers)
        {
            std::wstring buffer{LR"([user]
[boot]
command = /usr/libexec/wsl-systemd

)"};
            std::wstringstream fakeFile{buffer};
            ASSERT_TRUE(ini_find_value(fakeFile, L"boot", L"command", L"/usr/libexec/wsl-systemd"));
        }
        TEST(IniFindValueTests, LocalhostForwarding)
        {
            std::wstring buffer{LR"([wsl2]
# Turn off default connection to bind WSL 2 localhost to Windows localhost
localhostforwarding=true
)"};
            std::wstringstream fakeFile{buffer};
            ASSERT_FALSE(ini_find_value(fakeFile, L"wsl2", L"localhostforwarding", L"false"));
        }
        TEST(IniFindValueTests, LocalhostForwarding2)
        {
            std::wstring buffer{LR"([wsl2]
# Turn off default connection to bind WSL 2 localhost to Windows localhost
localhostforwarding=false
)"};
            std::wstringstream fakeFile{buffer};
            ASSERT_TRUE(ini_find_value(fakeFile, L"wsl2", L"localhostforwarding", L"false"));
        }
        TEST(IniFindValueTests, LocalhostForwarding3)
        {
            std::wstring buffer{LR"([wsl2]
# Turn off default connection to bind WSL 2 localhost to Windows localhost
)"};
            std::wstringstream fakeFile{buffer};
            ASSERT_FALSE(ini_find_value(fakeFile, L"wsl2", L"localhostforwarding", L"false"));
        }
    }
}
