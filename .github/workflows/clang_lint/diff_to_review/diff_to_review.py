# /usr/bin/python
""" Parses unified diff string and generate pull request review comments.
This is meant to run after formatters of any language in CI. The returned
dict is almost ready to be posted. Just need to add a new entry called
'event', which can be 'COMMENT' or 'REQUEST_CHANGES'.  """
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
from typing import List
from unidiff import PatchSet


def diff_to_review(repo_root: str, diff: str, review_msg_body: str,
                   inline_msg: str, source_files: List[str]):

    review_comments = []
    patch_set = PatchSet(diff)
    for patch in patch_set:
        file_path = patch.source_file.replace("a/", "")
        source_file = os.path.normpath(file_path)
        source_file = os.path.join(repo_root, source_file)
        if source_file not in source_files:
            continue
        for hunk in patch:
            start_line = hunk.source_start
            line_number = hunk.source_start + hunk.source_length - 1
            target_lines = [
                tl.value for tl in hunk.target_lines()
            ]
            review_comment_body = inline_msg + "\n\n```suggestion\n" + \
                "".join(target_lines) + "```"
            review_comments.append(
                {
                    "path": file_path,
                    "start_line": start_line,
                    "line": line_number,
                    "side": "RIGHT",
                    "body": review_comment_body,
                }
            )

    return {
        "body": review_msg_body,
        "comments": review_comments,
    }
