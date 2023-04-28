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
    /// A data structure defining a patchFn to be done to a specific configuration file.
    /// It's a dummy high level construct that allows a declarative approach to assign to configuration files that need
    /// patching the functions capable of doing so, allowing easy and decoupled implementation of such functions, as
    /// long as they obey the contract defined by the [PatchFn] function type.
    struct Patch
    {
        /// A patchFn function can either:
        /// 1. transform the contents of an existing config file or
        /// 2. create/truncate it.
        ///
        /// Either way, the caller will provide a input stream buffer iterator linked to an open file, if that exists,
        /// or an end-of-stream iterator otherwise, which is semantically equivalent of the file being empty.
        /// Whether the file doesn't exist or is empty shall not be concern of the patching function.
        ///
        /// Returns true on success, i.e. if there is anything we should write to the distro file system.
        using PatchFn = bool(std::istreambuf_iterator<char> original, std::ostream& modified);

        /// The non-translated Linux path to the config file to be patched.
        std::filesystem::path configFilePath;
        /// What is to be done with that file.
        PatchFn* patchFn;

        /// Applies the [patchFn] by instantiating a [Patcher] with the [pathPrefix] for translating distro filesystem
        /// paths to Windows paths. This function is the way higher level constructs are expected to access the
        /// functionality exposed by this component.
        bool apply(const std::filesystem::path& pathPrefix) const
        {
            assert(patchFn != nullptr);
            Patcher patcher{pathPrefix, configFilePath};
            return patcher(*patchFn);
        }

        /// Separates the action of patching file contents from file/filesystem access.
        /// This class concerns itself with opening and closing the files a patching function might require.
        /// It implements a callable interface to visit the [PatchFnPtr] variant and apply the patching action.
        /// That's why this is the Patcher.
        /// It's creation and destruction are meant to be cheap enough to allow creation of instances on demand, one
        /// instance per patchFn to be applied.
        class Patcher
        {
          private:
            /// The absolute file path to the config file to be patched already translated to Windows paths.
            std::filesystem::path translatedFilePath;

            /// The stream a patching function must write into. Always starts empty.
            std::stringstream modified;

            /// Applies the contents of the file stream to the destination file path inside the distro. It assumes it
            /// must write the file, thus must be private so that precondition is ensured prior to calling this method.
            bool commit();

            /// Calls [patchFn] function passing a read-only file stream buffer iterator linked to the
            /// [translatedFilePath]. If such path doesn't exist, it must be treated as if the file is empty. The
            /// iterator allows that semantics.
            bool handleCall(PatchFn& patchFn);

          public:
            /// Creates a new Patcher instance, storing [linuxFile] translated to Windows paths according to a
            /// [pathPrefix].
            Patcher(const std::filesystem::path& pathPrefix, const std::filesystem::path& linuxFile);

            /// Movable but not copyable due the underlying stream, which is not copyable.
            Patcher() = default;
            Patcher(Patcher&& other) = default;
            ~Patcher() = default;
            Patcher(const Patcher& other) = delete;
            Patcher operator=(const Patcher& other) = delete;

            /// Calls [patchFn] with the appropriate input and output parameters and commits the patching result if the
            /// said function reports success.
            bool operator()(PatchFn& patchFn)
            {
                if (!handleCall(patchFn)) {
                    return false;
                }
                return commit();
            }

            // Observers (made public mainly for testing, allowing observing if certain invariants hold.
            // They are likely to never be called in production code).

            /// Returns a copy of the translatedFilePath defined during construction.
            const std::filesystem::path& translatedPath() const
            {
                return translatedFilePath;
            }
        };
    };
};
