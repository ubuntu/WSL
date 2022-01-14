#!/usr/bin/python3
""" Parses the .clang-ignore file at the root of the repository
and/or the excluded_paths environment variable to determine which
source files should be excluded from linting process. """
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
#

import os
import glob
import re
from typing import List


def files_to_lint(repo_root: str, project_dir: str) -> List[str]:
    """ Lists all source files in repo_root/project_dir that should be
    linted based on an existing repo_root/.clang-ignore file and/or
    `exclude_paths` environment variable """
    extensions = ["h", "hpp", "c", "cpp", "cc", "hh", "cxx", "hx"]
    base_dir = project_dir
    sources = []
    for ext in extensions:
        sources += glob.glob("{}/{}/*.{}".format(repo_root, base_dir, ext))

    # Using getenv to let explicit the dependency on the env var set due
    # the action egor-tensin/clang-format.
    # No globbing is allowed. The syntax is literal: file1:file2:file3
    excludes = []
    exclude_paths = os.getenv("exclude_paths")
    if exclude_paths is not None:
        excludes += exclude_paths.split(":")

    # Additionally (and preferrably), we can use a .clang-ignore file
    # with a syntax pretty similar to .gitignore, located at the top of the
    # repository tree. The syntax must be globbing friendly is not checked.
    comment_re = re.compile(r'^\s*\#+.+')
    empty_re = re.compile(r'^\s*$')
    ct_ignore = os.path.join(repo_root, base_dir, '.clang-ignore')
    if os.path.exists(ct_ignore):
        with open(ct_ignore, 'r') as f:
            for line in f.readlines():
                if empty_re.match(line) is None and \
                   comment_re.match(line) is None:
                    excludes += glob.glob("{}/{}/{}".format(repo_root,
                                                            base_dir,
                                                            line.strip()))

    sources = [os.path.normpath(s) for s in sources]
    excludes = [os.path.normpath(e) for e in excludes]
    # sources_to_lint will contain the non-excluded files.
    sources_to_lint = []
    for s in sources:
        if s not in excludes:
            sources_to_lint.append(os.path.normpath(s))

    return sources_to_lint
