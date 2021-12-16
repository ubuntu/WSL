#include "stdafx.h"
#include <regex>

namespace Oobe
{
    namespace internal
    {
        // The parsing function attempts to read the launcher file in a type safe way.
        // Should new requirements need to extend the parsing grammar, here is what needs to be done:
        // 1. Add new entries in the SupportedTypes enum and respective cases in the switch statement of the
        // `parseExitStatusFile()` function.
        // 2. Map the expected keys in the launcher file to the expected value types in the grammar map.
        enum class SupportedTypes
        {
            UInt,
            Double,
            String
        };
        const std::map<std::string, SupportedTypes> grammar{{"action", SupportedTypes::String},
                                                            {"defaultUid", SupportedTypes::UInt}};

        nonstd::expected<KeyValuePairs, const wchar_t*> parseExitStatusFile(std::istream& file)
        {
            const std::regex keyValueRe(R"(^\s*(\w+)\s*[=:]\s*(\w+).*$)",
                                  std::regex_constants::icase | std::regex_constants::ECMAScript);
            const std::regex commentRe(R"(^\s*#+.*)", std::regex_constants::icase | std::regex_constants::ECMAScript);
            std::string line;
            std::smatch match;
            KeyValuePairs parsed;
            while (std::getline(file, line)) {
                if (std::regex_search(line, match, commentRe)) {
                    continue; // It's a comment. Nothing to do.
                }
                if (std::regex_search(line, match, keyValueRe)) {
                    if (match.size() != 3) {
                        // Ill-formed. There should be only three matches: the whole line, the key and value.
                        continue;
                    }
                    auto key = match[1].str();
                    auto value = match[2].str();
                    try {
                        auto grammarIt = grammar.find(key);
                        if (grammarIt == grammar.end()) {
                            // Fail silently on unsupported keys.
                            continue;
                        }
                        switch (auto valueType = grammarIt->second; valueType) {
                        case SupportedTypes::String:
                            parsed[key] = value;
                            break;
                        case SupportedTypes::UInt:
                            parsed[key] = std::stoul(value);
                            break;
                        case SupportedTypes::Double:
                            parsed[key] = std::stod(value);
                            break;
                        default:
                            // unsupported stuff is just ignored.
                            break;
                        }
                        // Keep the strategy of silent failure.
                        // std::map promises that if an exception is thrown by any operation, the insertion has no
                        // effect
                    } catch ([[maybe_unused]] const std::invalid_argument& ex) {
                        // It would be nice to log it, but no other action is required.
                    } catch ([[maybe_unused]] const std::out_of_range& ex) {
                        // It would be nice to log it, but no other action is required.
                    }
                }
            }
            // In the end of this while loop we should have the relevant contents of that file inside this map.
            if (parsed.empty()) {
                return nonstd::make_unexpected(L"Failed parsing the launcher command file.");
            }

            return parsed;
        }
    }
}