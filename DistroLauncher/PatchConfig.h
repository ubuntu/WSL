#pragma once
/*
 * Copyright (C) 2023 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

namespace Ubuntu
{
    /// A function that updates an existing config file. It must read the contents from the [original] file stream and
    /// perform any updates to the [modified] stream. Returns true on success.
    using ModifyFn = bool(std::istream& original, std::ostream& modified);
    /// A function whose patching action takes place by creating a new file or completely overriding an existing file
    /// despite its contents. Returns true on success.
    using CreateFn = bool(std::ostream& modified);

    /// A patch function can be either one of the above types of functions. Both streams are passed already open and
    /// will be closed after that function returns, so no file management operations are of these function types
    /// concerns.
    using PatchFnPtr = std::variant<CreateFn*, ModifyFn*>;

    /// Separates the action of patching file contents from file/filesystem access.
    /// This class concerns itself with opening and closing the files a patching function might require.
    /// It implements a callable interface to visit the [PatchFnPtr] variant and apply the patching action.
    /// That's why this is the Patcher.
    /// It's creation and destruction are meant to be cheap enough to allow creation of instances on demand, one
    /// instance per patch to be applied.
    class Patcher
    {
      private:
        /// The absolute file path to the config file to be patched already translated to Windows paths.
        std::filesystem::path translatedFilePath;

        /// The stream a patching function must write into. Always starts empty.
        std::stringstream modified;

        /// Applies the contents of the file stream to the destination file path inside the distro. It assumes it must
        /// write the file, thus must be private so that precondition is ensured prior to calling this method.
        bool commit();

      public:
        /// Creates a new Patcher instance, storing [linuxFile] translated to Windows paths according to a [pathPrefix].
        Patcher(const std::filesystem::path& pathPrefix, const std::filesystem::path& linuxFile);

        /// Movable but not copyable due the underlying stream, which is not copyable.
        Patcher() = default;
        Patcher(Patcher&& other) = default;
        ~Patcher() = default;
        Patcher(const Patcher& other) = delete;
        Patcher operator=(const Patcher& other) = delete;

        /// Calls a [CreateFn] patching function. If the supplied callable succeeds, the modifications on the output
        /// file stream are commited to the original config file.
        inline bool operator()(CreateFn* patchFn)
        {
            if (!patchFn(modified)) {
                return false;
            }
            return commit();
        }

        /// Calls a [ModifyFn] patching function passing a read-only file stream pointing to the [configfilePath]. If
        /// the supplied callable succeeds, the modifications on the output file stream are commited to the original
        /// config file.
        inline bool operator()(ModifyFn* patchFn)
        {
            // let the destructor close it automatically.
            std::ifstream original{translatedFilePath};
            if (original.fail()) {
                return false;
            }
            if (!patchFn(original, modified)) {
                return false;
            }
            return commit();
        }

        // Observers (made public mainly for testing, allowing observing if certain invariants hold.
        // They are likely to never be called in production code).

        /// Returns a copy of the translatedFilePath defined during construction.
        std::filesystem::path translatedPath() const
        {
            return translatedFilePath;
        }
    };

    /// A data structure defining a patch to be done to a specific configuration file.
    struct PatchConfig
    {
        /// The non-translated Linux path to the config file to be patched. To avoid lifetime issues, make sure to point
        /// to static data, i.e. string literals.
        std::string_view configFilePath;
        /// What is to be done with that file.
        PatchFnPtr patch;
    };

    /// Applies the [patch] by instantiating a [Patcher] with the [pathPrefix] for translating distro filesystem paths
    /// to Windows paths. This function is the way higher level constructs are expected to access the functionality
    /// exposed by this component.
    inline bool apply(const PatchConfig& patch, const std::filesystem::path& pathPrefix)
    {
        return std::visit(Patcher{pathPrefix, patch.configFilePath}, patch.patch);
    }
};
