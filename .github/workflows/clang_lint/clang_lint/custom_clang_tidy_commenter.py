#!/usr/bin/python3
""" Creates pull request review comments from the exported diagnostics
results of a previous clang-tidy run. """
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
# Source code adapted from https://github.com/platisd/clang-tidy-pr-comments.

import os
import yaml

from clang_lint.inline_comments.markdown import markdown


def parse_clang_tidy_fixes(fixes_path: str, repository_root: str):
    """Produces a list of comment json objects by parsing the fixes file
    located at `fixes_path` exported by a previous clang-tidy run. """
    clang_tidy_fixes = dict()
    with open(fixes_path) as file:
        clang_tidy_fixes = yaml.full_load(file)

    if (
        clang_tidy_fixes is None
        or "Diagnostics" not in clang_tidy_fixes.keys()
        or len(clang_tidy_fixes["Diagnostics"]) == 0
    ):
        print("No warnings found by clang-tidy")
        return None

    # If it is a clang-tidy-8 format, then upconvert it to the clang-tidy-9 one
    if "DiagnosticMessage" not in clang_tidy_fixes["Diagnostics"][0].keys():
        clang_tidy_fixes["Diagnostics"][:] = [
            {
                "DiagnosticName": d["DiagnosticName"],
                "DiagnosticMessage": {
                    "FileOffset": d["FileOffset"],
                    "Message": d["Message"],
                    "Replacements": d["Replacements"],
                    "FilePath": d["FilePath"]
                },
            }
            for d in clang_tidy_fixes["Diagnostics"]
        ]

    # Clean up possible compilation failure garbage
    clang_tidy_fixes["Diagnostics"][:] = [
        {
            "DiagnosticName": d["DiagnosticName"],
            "DiagnosticMessage": d["DiagnosticMessage"]
        }
        for d in clang_tidy_fixes["Diagnostics"]
        if len(d["DiagnosticMessage"]["FilePath"]) > 0
     ]
    # Normalize paths
    for diagnostic in clang_tidy_fixes["Diagnostics"]:
        diagnostic["DiagnosticMessage"]["FilePath"] = os.path.normpath(
            diagnostic["DiagnosticMessage"]["FilePath"]
        )
        for replacement in diagnostic["DiagnosticMessage"]["Replacements"]:
            replacement["FilePath"] = os.path.normpath(
                replacement["FilePath"])
    # Create a separate diagnostic entry for each replacement entry, if any
    clang_tidy_diagnostics = list()
    for diagnostic in clang_tidy_fixes["Diagnostics"]:
        if not diagnostic["DiagnosticMessage"]["Replacements"]:
            clang_tidy_diagnostics.append(
                {
                    "DiagnosticName": diagnostic["DiagnosticName"],
                    "Message": diagnostic["DiagnosticMessage"]["Message"],
                    "FilePath": diagnostic["DiagnosticMessage"]["FilePath"],
                    "FileOffset": diagnostic["DiagnosticMessage"]["FileOffset"]
                }
            )
            continue
        for replacement in diagnostic["DiagnosticMessage"]["Replacements"]:
            clang_tidy_diagnostics.append(
                {
                    "DiagnosticName": diagnostic["DiagnosticName"],
                    "Message": diagnostic["DiagnosticMessage"]["Message"],
                    "FilePath": replacement["FilePath"],
                    "FileOffset": replacement["Offset"],
                    "ReplacementLength": replacement["Length"],
                    "ReplacementText": replacement["ReplacementText"],
                }
            )
    # Mark duplicates
    unique_diagnostics = set()
    for diagnostic in clang_tidy_diagnostics:
        unique_combo = (
            diagnostic["DiagnosticName"],
            diagnostic["FilePath"],
            diagnostic["FileOffset"],
        )
        diagnostic["Duplicate"] = unique_combo in unique_diagnostics
        unique_diagnostics.add(unique_combo)
    # Remove the duplicates
    clang_tidy_diagnostics[:] = [
        diagnostic
        for diagnostic in clang_tidy_diagnostics
        if not diagnostic["Duplicate"]
    ]

    # Create the review comments
    review_comments = list()
    for diagnostic in clang_tidy_diagnostics:
        file_path = diagnostic["FilePath"]
        if len(file_path) == 0:
            continue
        line_number = 0
        suggestion = ""
        with open(os.path.join(repository_root, file_path)) as source_file:
            source_file.seek(diagnostic["FileOffset"])
            offset_line_ending = source_file.readline()
            character_counter = 0
            source_file.seek(0)
            for source_file_line in source_file:
                character_counter += len(source_file_line)
                line_number += 1
                # Check if we have found the line with the warning
                replacement_begin = source_file_line.find(offset_line_ending)
                if replacement_begin != -1:
                    # beginning_of_line = character_counter - len(
                    #     source_file_line)
                    if "ReplacementText" in diagnostic:
                        # The offset from the start of line until the warning
                        # replacement_begin = diagnostic["FileOffset"] - \
                        #     beginning_of_line
                        # Crazy hack due char vs byte counts enconding fight.
                        # current_position = source_file.tell()
                        source_file.seek(diagnostic["FileOffset"] +
                                         diagnostic["ReplacementLength"], 0)
                        rest_of_the_line = source_file.readline()
                        # I guess the line below is not actually needed,
                        # but just in case I forgot to test that theses further
                        # source_file.seek(current_position, 0)
                        source_file_line = (
                            source_file_line[: replacement_begin]
                            + diagnostic["ReplacementText"]
                            + rest_of_the_line
                        )
                        # Make sure the code suggestion ends with a \n
                        if source_file_line[-1] != "\n":
                            source_file_line += "\n"
                        suggestion = "\n```suggestion\n" + \
                            source_file_line + "```"
                    break

        review_comment_body = (
            ":warning: **"
            + markdown(diagnostic["DiagnosticName"])
            + "** :warning:\n"
            + markdown(diagnostic["Message"])
            + suggestion
        )
        review_comments.append(
            {
                "path": os.path.relpath(file_path, repository_root)
                .replace("\\", "/"),
                "line": line_number,
                "side": "RIGHT",
                "body": review_comment_body,
            }
        )

    return review_comments


def review_comments(repository_root: str, pull_request_id: int,
                    export_fixes_file: str):
    """Generates the PR review object, consisting of a body and a list 
    of comments."""
    review_comments = parse_clang_tidy_fixes(export_fixes_file,
                                             repository_root)
    if len(review_comments) == 0:
        print("Unexpected lack of comments from clang-tidy-fixes happen.")
        return None

    warning_comment = ":warning: Please check out carefully the isses found " \
                      "by `clang-tidy` in the new code :warning:\n"

    return {
        "body": warning_comment,
        "comments": review_comments,
    }
