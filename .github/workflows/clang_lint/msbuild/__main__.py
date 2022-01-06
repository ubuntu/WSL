# /usr/bin/python
""" Invoked by msbuild to run in behalf of the actual clang-tidy.exe
to accept the command lines --export-fixes and --line-filter. """
#
# Copyright (C) 2021 Canonical Ltd
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#


import sys
from . import clang_tidy_runner

if __name__ == "__main__":
    if len(sys.argv) > 1:
        clang_tidy_runner.run(*sys.argv[1:])
