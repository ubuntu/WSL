#include "stdafx.h"
#include <sstream>

namespace Oobe::internal
{
    namespace
    {
        const wchar_t* const whitespaces = L" \t\n\r\f\v";
    }
    /// Trims both ends of a string in place.
    inline void trim(std::wstring& str)
    {

        auto lastpos = str.find_last_not_of(whitespaces);
        if (lastpos == std::wstring::npos) {
            str.clear();
            return;
        }

        str.erase(lastpos + 1);
        str.erase(0, str.find_first_not_of(whitespaces));
    }

    /// Returns true if the argument contains white spaces in the beginning or in the end.
    inline bool has_surrounding_whitespaces(std::wstring& str)
    {
        return str.find_first_not_of(whitespaces) != 0 || str.find_last_not_of(whitespaces) + 1 != str.length();
    }
    /// Removes wrapping square brackets in place.
    /// Returns false if the string didn't contain the wrapping bracket pair.
    inline bool remove_surrounding_brackets(std::wstring& str)
    {
        if (str[0] != L'[') {
            return false;
        }
        auto closingBracketPos = str.find_last_of(L']');
        if (closingBracketPos == std::wstring::npos) {
            return false;
        }

        str.erase(closingBracketPos);
        str.erase(0, 1); // removes the first character only.
        return true;
    }

    /// Returns true if the entry [section.key] is present in the [ini] file stream and its value contains the string
    /// [valueContains]. This is intended to be called once, thus no caching.
    bool ini_find_value(std::wistream& ini, std::wstring_view section, std::wstring_view key,
                        std::wstring_view valueContains)
    {
        std::wstring line;
        std::wistringstream splitter;
        std::wstring currentSection;
        std::wstring currentKey;
        std::wstring value;
        ini.seekg(0);
        while (std::getline(ini, line)) {
            // skip empty lines.
            if (line.empty()) {
                continue;
            }
            trim(line);
            switch (line[0]) {
            case L'#': // skip comments.
                break;

            case L'[':
                // ill-formed section header breaks parsing.
                if (!remove_surrounding_brackets(line)) {
                    return false;
                }
                if (has_surrounding_whitespaces(line)) {
                    // WSL treats spaces in the beginning or in the end of the section name as syntax error.
                    return false;
                }
                currentSection = line;
                break;

            default: // it has to be a key=value line.
                // If there was no single '=' character in this line, most probably there is a syntax error and the
                // configuration file is ill-formed. Thus, WSL would have ignored the rest of the file, so it doesn't
                // matter whether we would find systemd activation line or not after this point or not. WSL current
                // implementation stops parsing the file after a parsing error is found, yet it performs the
                // configuration steps of the previously validated entries. There might be billions of other ways to
                // break the syntax. It's probably not worthy attempting to prevent them all.
                if (line.find_first_of(L'=') == std::wstring::npos) {
                    return false;
                }
                splitter.clear();
                splitter.str(line);
                std::getline(splitter, currentKey, L'=');
                std::getline(splitter, value, L'=');
                trim(currentKey);
                trim(value);

                // The only possible way to return true from this function is finding the key=*valueContains* definition
                // we are looking for inside the proper section.
                if (currentSection == section && currentKey == key && value.find(valueContains) != std::wstring::npos) {
                    return true;
                }
            }



            
        }

         return false;
    } // bool ini_find_value()

} // Oobe::internal
