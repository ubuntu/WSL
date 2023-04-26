#include "stdafx.h"
#include "gtest/gtest.h"
#include "PatchConfig.h"

namespace Ubuntu
{
    TEST(PatchConfig, PathTranslation)
    {
        // A typical distro path prefix on Windows 11
        const auto* win11Prefix = LR"(\\wsl.localhost\Ubuntu22.04LTS)";
        const auto* linuxFile = "/root/here-I-am";
        Patcher patcher{win11Prefix, linuxFile};
        EXPECT_EQ(patcher.translatedPath(), std::filesystem::path{R"(\\wsl.localhost\Ubuntu22.04LTS\root\here-I-am)"});
    }
    TEST(PatchConfig, PathTranslation2)
    {
        // A typical distro path prefix on Windows 10
        const auto* win10Prefix = LR"(\\wsl$\Ubuntu18.04LTS)";
        const auto* linuxFile = "/root/here-I-am";
        Patcher patcher{win10Prefix, linuxFile};
        EXPECT_EQ(patcher.translatedPath(), std::filesystem::path{R"(\\wsl$\Ubuntu18.04LTS\root\here-I-am)"});
    }
    TEST(PatchConfig, PathTranslation3)
    {
        // Simulates a prefix chosen for testing independently of WSL.
        const auto* win10Prefix = LR"(C:\\Temp)";
        const auto* linuxFile = "/root/here-I-am";
        Patcher patcher{win10Prefix, linuxFile};
        EXPECT_EQ(patcher.translatedPath(), std::filesystem::path{R"(C:\\Temp\root\here-I-am)"});
    }

    enum class SampleStrings
    {
        systemd,
        wslConfAppend,
        wslConfOriginal,
    };
    const char* sampleContents(SampleStrings which)
    {
        switch (which) {
        case SampleStrings::systemd:
            return "[Unit]\nDisable=Forever\n";
        case SampleStrings::wslConfAppend:
            return "[boot]\nsystemd=true";
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
    class PatchConfigTest : public ::testing::Test
    {
      protected:
        std::filesystem::path prefix;
        std::optional<PatchConfig> patchConfig;
        // Test setup
        PatchConfigTest() : prefix{Win32Utils::thisAppRootdir()}
        {
        }
        // Deletes any directories created during tests.
        ~PatchConfigTest() override
        {
            if (patchConfig.has_value()) {

                std::filesystem::path linuxFile{patchConfig.value().configFilePath};
                auto cleanUpDir{prefix / *(linuxFile.relative_path().begin())};
                std::error_code err;
                std::filesystem::remove_all(cleanUpDir, err);
            }
        }

        /* TEST HELPERS */

        // Returns the path where the configuration etcWslConf should be written to according to the [patchConfig]
        // value.
        std::filesystem::path expectedFile() const
        {
            assert(patchConfig.has_value());
            return prefix / std::filesystem::path{patchConfig.value().configFilePath}.relative_path();
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

    TEST_F(PatchConfigTest, ApplyCreationPatch)
    {
        // Setup
        patchConfig = PatchConfig{
          "/etc/systemd/system/funny.service.d/00-wsl.conf",
          [](std::ostream& conf) {
              std::string_view contents{sampleContents(SampleStrings::systemd)};
              conf.write(contents.data(), contents.length());
              return !conf.fail();
          },
        };
        // Act
        apply(patchConfig.value(), prefix);

        // Assert
        std::ifstream result{expectedFile()};
        EXPECT_FALSE(result.fail());
        std::string content(std::istreambuf_iterator<char>{result}, std::istreambuf_iterator<char>{});
        EXPECT_EQ(content, sampleContents(SampleStrings::systemd));
    }
    TEST_F(PatchConfigTest, ApplyModPatch)
    {
        // Setup
        patchConfig = PatchConfig{
          "/etc/wsl.conf",
          [](std::istream& conf, std::ostream& mod) {
              // copy original contents verbatim.
              std::copy(std::istreambuf_iterator<char>(conf), std::istreambuf_iterator<char>{},
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
        apply(patchConfig.value(), prefix);

        // Assert
        std::ifstream result{expectedFile()};
        EXPECT_FALSE(result.fail());
        std::string content(std::istreambuf_iterator<char>{result}, std::istreambuf_iterator<char>{});
        std::string expectedContent{sampleContents(SampleStrings::wslConfOriginal)};
        expectedContent.append(sampleContents(SampleStrings::wslConfAppend));

        EXPECT_EQ(content, expectedContent);
    }
    TEST_F(PatchConfigTest, ApplyRewritePatch)
    {
        // Setup
        patchConfig = PatchConfig{
          "/etc/wsl.conf",
          [](std::istream& conf, std::ostream& mod) {
              // All new stuff, original contents simply disregarded.
              std::string_view contents{sampleContents(SampleStrings::wslConfAppend)};
              mod.write(contents.data(), contents.length());
              return !mod.fail();
          },
        };
        // Makes the /etc/wsl.conf file exist before patching with the fake original contents.
        makeExpectedFile(sampleContents(SampleStrings::wslConfOriginal));

        // Act
        apply(patchConfig.value(), prefix);

        // Assert
        std::ifstream result{expectedFile()};
        EXPECT_FALSE(result.fail());
        std::string content(std::istreambuf_iterator<char>{result}, std::istreambuf_iterator<char>{});
        EXPECT_EQ(content, sampleContents(SampleStrings::wslConfAppend));
    }
}
