#include "stdafx.h"
#include "gtest/gtest.h"
#include "Patch.h"

namespace Ubuntu
{
    TEST(Patch, PathTranslation)
    {
        // A typical distro path prefix on Windows 11
        const auto* win11Prefix = LR"(\\wsl.localhost\Ubuntu22.04LTS)";
        const auto* linuxFile = "/root/here-I-am";
        Patch::Patcher patcher{win11Prefix, linuxFile};
        EXPECT_EQ(patcher.translatedPath(), std::filesystem::path{R"(\\wsl.localhost\Ubuntu22.04LTS\root\here-I-am)"});
    }
    TEST(Patch, PathTranslation2)
    {
        // A typical distro path prefix on Windows 10
        const auto* win10Prefix = LR"(\\wsl$\Ubuntu18.04LTS)";
        const auto* linuxFile = "/root/here-I-am";
        Patch::Patcher patcher{win10Prefix, linuxFile};
        EXPECT_EQ(patcher.translatedPath(), std::filesystem::path{R"(\\wsl$\Ubuntu18.04LTS\root\here-I-am)"});
    }
    TEST(Patch, PathTranslation3)
    {
        // Simulates a prefix chosen for testing independently of WSL.
        const auto* win10Prefix = LR"(C:\\Temp)";
        const auto* linuxFile = "/root/here-I-am";
        Patch::Patcher patcher{win10Prefix, linuxFile};
        EXPECT_EQ(patcher.translatedPath(), std::filesystem::path{R"(C:\\Temp\root\here-I-am)"});
    }

    enum class SampleStrings
    {
        confComment,
        fstab1804,
        randomFstab,
        systemd,
        wslConfAppend,
        wslConfOriginal,
    };
    const char* sampleContents(SampleStrings which)
    {
        switch (which) {
        case SampleStrings::confComment:
            return "# This is a comment.\n";
        case SampleStrings::fstab1804:
            // copy-pasted from `hexdump -c /etc/fstab` on 18.04
            return "LABEL=cloudimg-rootfs\t/\t ext4\tdefaults\t0 1\n";
        case SampleStrings::randomFstab:
            return R"(# <file system>        <dir>         <type>    <options>             <dump> <pass>
LABEL=Debian    /    ext4   defaults    1 0
)";
        case SampleStrings::systemd:
            return "[Unit]\nDisable=Forever\n";
        case SampleStrings::wslConfAppend:
            return "[boot]\nsystemd=true\n";
        case SampleStrings::wslConfOriginal:
            return R"(
[user]
defaultUid=1000

[mount]
options=metadata
)";
        default:
            return "";
        }
    }

    // A base class for the subsequent test fixtures declared via the TEST_F macro.
    class PatchTest : public ::testing::Test
    {
      protected:
        std::filesystem::path prefix;
        std::optional<Patch> patch;
        // Test setup
        PatchTest() : prefix{Win32Utils::thisAppRootdir()}
        {
        }
        // Deletes any directories created during tests.
        ~PatchTest() override
        {
            if (patch.has_value()) {

                std::filesystem::path linuxFile{patch.value().configFilePath};
                auto cleanUpDir{prefix / *(linuxFile.relative_path().begin())};
                std::error_code err;
                std::filesystem::remove_all(cleanUpDir, err);
            }
        }

        /* TEST HELPERS */

        // Returns the path where the configuration etcWslConf should be written to according to the [patchFn]
        // value.
        std::filesystem::path expectedFile() const
        {
            assert(patch.has_value());
            return prefix / std::filesystem::path{patch.value().configFilePath}.relative_path();
        }

        // Creates a fake file in the expected path with the supplied [contents].
        bool makeExpectedFile(std::string_view contents) const
        {
            auto path = expectedFile();
            std::filesystem::create_directories(path.parent_path());
            std::ofstream file{path};
            file.write(contents.data(), contents.length());
            return !file.fail();
        }
    };

    TEST_F(PatchTest, ApplyCreationPatch)
    {
        // Setup
        patch = Patch{
          "/etc/systemd/system/funny.service.d/00-wsl.conf",
          [](std::istreambuf_iterator<char> original, std::ostream& conf) {
              std::string_view contents{sampleContents(SampleStrings::systemd)};
              conf.write(contents.data(), contents.length());
              return !conf.fail();
          },
        };
        // Act
        patch.value().apply(prefix);

        // Assert
        std::ifstream result{expectedFile()};
        EXPECT_FALSE(result.fail());
        std::string content(std::istreambuf_iterator<char>{result}, std::istreambuf_iterator<char>{});
        EXPECT_EQ(content, sampleContents(SampleStrings::systemd));
    }
    TEST_F(PatchTest, ApplyModPatch)
    {
        // Setup
        patch = Patch{
          "/etc/wsl.conf",
          [](std::istreambuf_iterator<char> original, std::ostream& mod) {
              // copy original contents verbatim.
              std::copy(original, std::istreambuf_iterator<char>{},
                        std::ostreambuf_iterator<char>(mod));
              // Appends new stuff
              std::string_view contents{sampleContents(SampleStrings::wslConfAppend)};
              mod.write(contents.data(), contents.length());
              return !mod.fail();
          },
        };
        // Makes the /etc/wsl.conf file exist before patching with the fake original contents.
        makeExpectedFile(sampleContents(SampleStrings::wslConfOriginal));

        // Act
        patch.value().apply(prefix);

        // Assert
        std::ifstream result{expectedFile()};
        EXPECT_FALSE(result.fail());
        std::string content(std::istreambuf_iterator<char>{result}, std::istreambuf_iterator<char>{});
        std::string expectedContent{sampleContents(SampleStrings::wslConfOriginal)};
        expectedContent.append(sampleContents(SampleStrings::wslConfAppend));

        EXPECT_EQ(content, expectedContent);
    }
    TEST_F(PatchTest, ApplyRewritePatch)
    {
        // Setup
        patch = Patch{
          "/etc/wsl.conf",
          [](std::istreambuf_iterator<char> original, std::ostream& mod) {
              // All new stuff, original contents simply disregarded.
              std::string_view contents{sampleContents(SampleStrings::wslConfAppend)};
              mod.write(contents.data(), contents.length());
              return !mod.fail();
          },
        };
        // Makes the /etc/wsl.conf file exist before patching with the fake original contents.
        makeExpectedFile(sampleContents(SampleStrings::wslConfOriginal));

        // Act
        patch.value().apply(prefix);

        // Assert
        std::ifstream result{expectedFile()};
        EXPECT_FALSE(result.fail());
        std::string content(std::istreambuf_iterator<char>{result}, std::istreambuf_iterator<char>{});
        EXPECT_EQ(content, sampleContents(SampleStrings::wslConfAppend));
    }
    /* Patching functions tests - asserts their behavior */

    TEST(PatchingFn, CloudImgLabel)
    {
        // Makes the /etc/fstab exactly like 18.04's
        std::istringstream input(sampleContents(SampleStrings::fstab1804));
        std::stringstream output;
        PatchingFunctions::RemoveCloudImgLabel(input, output);
        // the patch function should have removed the only line the file contained.
        EXPECT_EQ(output.str().size(), 0);
    }
    TEST(PatchingFn, CloudImgLabelLeadingSpaces)
    {
        std::string slightlyChanged{sampleContents(SampleStrings::confComment)};
        slightlyChanged.append("    ");
        slightlyChanged.append(sampleContents(SampleStrings::fstab1804));
        // slightlyChanged is now:
        /*

        # This is a comment.
        LABEL=cloudimg-rootfs\t/\t ext4\tdefaults\t0 1\n

        */
        std::istringstream input(slightlyChanged);
        std::stringstream output;

        // Apply
        PatchingFunctions::RemoveCloudImgLabel(input, output);

        // Assert
        // the patch function should have preserved the other line, which is just a comment.
        EXPECT_EQ(output.str(), sampleContents(SampleStrings::confComment));
    }
    TEST(PatchingFn, CloudImgLabelThirdLine)
    {
        std::string slightlyChanged{sampleContents(SampleStrings::confComment)};
        slightlyChanged.append(sampleContents(SampleStrings::fstab1804));
        slightlyChanged.append(sampleContents(SampleStrings::randomFstab));
        // slightlyChanged is now 4 lines and the second should be skipped.

        std::istringstream input(slightlyChanged);
        std::stringstream output;

        // Apply
        PatchingFunctions::RemoveCloudImgLabel(input, output);

        // Assert
        // the patch function should have preserved the other line, which is just a comment.
        std::string expected{sampleContents(SampleStrings::confComment)};
        expected.append(sampleContents(SampleStrings::randomFstab));
        EXPECT_EQ(output.str(), expected);
    }
    TEST(PatchingFn, CloudImgRandomFstab)
    {
        // Makes the /etc/fstab unrelated to 18.04's, thus it must be preserved as is.
        std::istringstream input(sampleContents(SampleStrings::randomFstab));
        std::stringstream output;
        PatchingFunctions::RemoveCloudImgLabel(input, output);
        // Must be unchanged
        EXPECT_EQ(output.str(), input.str());
    }
    TEST(PatchingFn, EnableSystemdNewFile)
    {
        std::istringstream input{}; // sample wsl.conf file
        std::stringstream output;
        PatchingFunctions::EnableSystemd(input, output);

        EXPECT_EQ(output.str(), "\n[boot]\nsystemd=true\n");
    }
    TEST(PatchingFn, EnableSystemdAppend)
    {
        std::istringstream input("[interop]\nenabled=true"); // sample wsl.conf file
        std::stringstream output;
        PatchingFunctions::EnableSystemd(input, output);

        EXPECT_EQ(output.str(), "[interop]\nenabled=true\n[boot]\nsystemd=true\n");
    }
    TEST(PatchingFn, DefaultUpgradePolicy)
    {
        std::istringstream input("This is some\ntext that\n   should work as a sample\n     Prompt=WRONG\n some trailing text");
        std::stringstream output;
        PatchingFunctions::SetDefaultUpgradePolicy(input, output);

        EXPECT_EQ(output.str(),
                  "This is some\ntext that\n   should work as a sample\n     Prompt=normal\n some trailing text");
    }
    TEST(PatchingFn, DeferRebootNewFile)
    {
        std::istringstream input{};
        std::stringstream output;
        PatchingFunctions::DeferReboot(input, output);

        EXPECT_EQ(output.str(), "\naction=reboot\n");
    }
    TEST(PatchingFn, DeferRebootAppend)
    {
        std::istringstream input("this is some sample text");
        std::stringstream output;
        PatchingFunctions::DeferReboot(input, output);

        EXPECT_EQ(output.str(), "this is some sample text\naction=reboot\n");
    }
    TEST(PatchingFn, SysUsersDisableLoadCredentialNewFile)
    {
        std::istringstream input{};
        std::stringstream output;
        PatchingFunctions::SysUsersDisableLoadCredential(input, output);

        EXPECT_EQ(output.str(), "\n[Service]\nLoadCredential=\n");
    }
    TEST(PatchingFn, SysUsersDisableLoadCredentialAppend)
    {
        std::istringstream input("this is some sample text");
        std::stringstream output;
        PatchingFunctions::SysUsersDisableLoadCredential(input, output);

        EXPECT_EQ(output.str(), "this is some sample text\n[Service]\nLoadCredential=\n");
    }

    /* Wiring tests - asserts the patching functions are associated with the distros and files as supposed. */

    // Comparison operator for PatchConfig aggregate - helps with find algorithms.
    bool operator==(const Patch& lhs, const Patch& rhs)
    {
        return lhs.patchFn == rhs.patchFn && lhs.configFilePath == rhs.configFilePath;
    }

    // Returns true if an equivalent [patchConfig] is registered for all distros.
    bool isGloballyRegisteredFor(const Patch& patchConfig)
    {
        auto it = std::find(releaseAgnosticPatches.begin(), releaseAgnosticPatches.end(), patchConfig);

        return it != releaseAgnosticPatches.end();
    }

    // Returns true if an equivalent [patchConfig] is registered for the [distro] specified.
    bool isRegisteredFor(std::wstring_view distro, const Patch& patchConfig)
    {
        auto patches = releaseSpecificPatches.find(distro);
        if (patches == releaseSpecificPatches.end()) {
            return false;
        }
        auto it = std::find(patches->second.begin(), patches->second.end(), patchConfig);

        return it != patches->second.end();
    }

    TEST(PatchWiringTest, CloudImgLabel)
    {
        // Makes sure the function PatchingFunctions::RemoveCloudImgLabel is associated with the file "/etc/fstab" for
        // all distros.
        EXPECT_TRUE(isGloballyRegisteredFor({"/etc/fstab", PatchingFunctions::RemoveCloudImgLabel}));
    }
    TEST(PatchWiringTest, CondVirt1804)
    {
        // Makes sure the function PatchingFunctions::OverrideUnitVirtualizationContainer is associated with the file
        // "/etc/systemd/system/systemd-modules-load.service.d/00-wsl.conf" for 18.04.
        EXPECT_TRUE(isRegisteredFor(L"Ubuntu-18.04",
                                    {"/etc/systemd/system/systemd-modules-load.service.d/00-wsl.conf",
                                     PatchingFunctions::OverrideUnitVirtualizationContainer}));
        // That function is currently so simple that, if we consider the rest of the patching feature well tested, then
        // there is no reason to test it.
    }
    TEST(PatchWiringTest, CondVirt2004)
    {
        // Makes sure the function PatchingFunctions::OverrideUnitVirtualizationContainer is associated with the file
        // "/etc/systemd/system/multipathd.socket.d/00-wsl.conf" for 18.04.
        EXPECT_TRUE(isRegisteredFor(L"Ubuntu-20.04", {
                                                       "/etc/systemd/system/multipathd.socket.d/00-wsl.conf",
                                                       PatchingFunctions::OverrideUnitVirtualizationContainer,
                                                     }));
    }
}
