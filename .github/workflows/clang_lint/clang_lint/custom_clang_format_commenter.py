#!/usr/bin/python3
""" Exports a function to run clang-format against the non-ignored
sources and turn the generated diff into review comments, if any. """
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


import subprocess
import git

from clang_lint.diff_to_review import diff_to_review
from clang_lint.clang_lint import files_to_lint


def review_comments(repository_root: str, pull_request_id: int,
                    review: str = "REQUEST_CHANGES",
                    fallback_style: str = "LLVM"):
    project_dir = "DistroLauncher"
    sources = files_to_lint.files_to_lint(repository_root, project_dir)
    proc = subprocess.run(["clang-format", "--fallback-style",
                           fallback_style, "-i", "--style", "file"] +
                          sources)
    if proc.returncode != 0:
        print("Failed to run clang-format.")
        return None

    repo = git.Repo(repository_root)
    kwargs = {"unified": 0}
    git_diff = repo.git.diff(**kwargs)
    if not git_diff:
        print("Formatting is fine. No issues found.")
        return 0

    body = ":warning: `clang-format` found formatting issues in the code" \
           " submitted.:warning:\nMake sure to run clang-format and update" \
           " this pull request.\n"
    inline_msg = "Clang-format suggestion below:"
    pr_review = diff_to_review.diff_to_review(repository_root, git_diff,
                                              body, inline_msg, sources)
    # Reset repo, otherwise the next one in the pipeline will work on a
    # modified tree different from what GitHub displays on browser.
    repo.git.reset('--hard')

    if pr_review is None:
        print("Failed to generate the review data from the clang-format diff.")
        return None

    return pr_review
