#!/usr/bin/python3
""" Posts pull request review comments, excluding the existing ones and
the ones not affecting files modified in the current pull_request_id."""
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

import itertools
import json
import os
import time
import re

import requests


def chunks(lst, n):
    # Copied from: https://stackoverflow.com/a/312464
    """Yield successive n-sized chunks from lst."""
    for i in range(0, len(lst), n):
        yield lst[i: i + n]


def _files_from_this_pr(github_api_url, repo, pull_request_id, github_token):
    """Lists which files and lines are allowed to receive comments, i.e.
    those modified by the current pull_request_id Pull Request."""
    pull_request_files = list()
    # Request a maximum of 100 pages (3000 files)
    for page_num in range(1, 101):
        pull_files_url = "%s/repos/%s/pulls/%d/files?page=%d" % (
            github_api_url,
            repo,
            pull_request_id,
            page_num,
        )

        pull_files_result = requests.get(
            pull_files_url,
            headers={
                "Accept": "application/vnd.github.v3+json",
                "Authorization": "token %s" % github_token,
            },
        )

        if pull_files_result.status_code != requests.codes.ok:
            print(
                "Request to get list of files failed with error code: "
                + str(pull_files_result.status_code)
            )
            return None

        pull_files_chunk = json.loads(pull_files_result.text)

        if len(pull_files_chunk) == 0:
            break

        pull_request_files.extend(pull_files_chunk)

    files_and_lines_available_for_comments = dict()

    for pull_request_file in pull_request_files:
        # Not all PR file metadata entries may contain a patch section
        # E.g., entries related to removed binary files may not contain it
        if "patch" not in pull_request_file:
            continue

        git_line_tags = re.findall(r"@@ -\d+,\d+ \+\d+,\d+ @@",
                                   pull_request_file["patch"])

        lines_and_changes = [
            line_tag.replace("@@", "").strip().split()[1].replace("+", "")
            for line_tag in git_line_tags
        ]

        lines_available_for_comments = [
            list(
                range(
                    int(change.split(",")[0]),
                    int(change.split(",")[0]) + int(change.split(",")[1]),
                )
            )
            for change in lines_and_changes
        ]

        lines_available_for_comments = list(
            itertools.chain.from_iterable(lines_available_for_comments)
        )
        files_and_lines_available_for_comments[
            pull_request_file["filename"]
        ] = lines_available_for_comments

    return files_and_lines_available_for_comments


def post_pr_review_comments(repository: str, pull_request_id: int,
                            review_comments: dict):
    """ Posts a PR Review event from each 15 review_comments which
    matching the output of `files_and_lines_available_for_comments`"""
    github_api_url = os.environ.get("GITHUB_API_URL")
    github_token = os.environ.get("INPUT_GITHUB_TOKEN")
    files_and_lines_available_for_comments = \
        _files_from_this_pr(github_api_url, repository,
                            pull_request_id, github_token)
    if files_and_lines_available_for_comments is None:
        print("Couldn't get the files of this PR from GitHub")
        return 1

    # Dismanteling the review_comments object for filtering purposes.
    review_body = review_comments["body"]
    review_event = review_comments["event"]
    comments = review_comments["comments"]
    actual_comments = dict()
    # Ignore comments on lines that were not changed in the pull request
    # Remove entries we cannot comment on as the files weren't changed in this
    # pull request
    actual_comments = [
        c
        for c in comments
        if c["path"]
        in files_and_lines_available_for_comments.keys()
        and c["line"] in files_and_lines_available_for_comments[c["path"]]
    ]

    # Load the existing review comments
    existing_pull_request_comments = list()
    # Request a maximum of 100 pages (3000 comments)
    for page_num in range(1, 101):
        pull_comments_url = "%s/repos/%s/pulls/%d/comments?page=%d" % (
            github_api_url,
            repository,
            pull_request_id,
            page_num,
        )
        pull_comments_result = requests.get(
            pull_comments_url,
            headers={
                "Accept": "application/vnd.github.v3+json",
                "Authorization": "token %s" % github_token,
            },
        )

        if pull_comments_result.status_code != requests.codes.ok:
            print(
                "Request to get pull request comments failed with error code: "
                + str(pull_comments_result.status_code)
            )
            return 1

        pull_comments_chunk = json.loads(pull_comments_result.text)

        if len(pull_comments_chunk) == 0:
            break

        existing_pull_request_comments.extend(pull_comments_chunk)

    # Exclude already posted comments
    for comment in existing_pull_request_comments:
        actual_comments = list(
            filter(
                lambda review_comment: not (
                    review_comment["path"] == comment["path"] and
                    review_comment["line"] == comment["line"] and
                    review_comment["side"] == comment["side"] and
                    review_comment["body"] == comment["body"]
                ),
                actual_comments,
            )
        )

    if len(actual_comments) == 0:
        print("No new warnings found for this pull request.")
        return 0

    # Split the comments in chunks to avoid overloading the server
    # and getting 502 server errors as a response for large reviews
    suggestions_per_comment = 15
    actual_comments = list(chunks(actual_comments, suggestions_per_comment))
    total_reviews = len(actual_comments)
    current_review = 1
    for comments_chunk in actual_comments:
        warning_comment = (
           (review_body + " (%i/%i)") % (current_review, total_reviews)
        )
        current_review += 1

        pull_request_reviews_url = "%s/repos/%s/pulls/%d/reviews" % (
            github_api_url,
            repository,
            pull_request_id,
        )
        post_review_result = requests.post(
            pull_request_reviews_url,
            json={
                "body": warning_comment,
                "event": review_event,
                "comments": comments_chunk,
            },
            headers={
                "Accept": "application/vnd.github.v3+json",
                "Authorization": "token %s" % github_token,
            },
        )

        if post_review_result.status_code != requests.codes.ok:
            print(post_review_result.text)
            # Ignore bad gateway errors (false negatives?)
            if post_review_result.status_code != requests.codes.bad_gateway:
                print(
                    "Posting review comments failed with error code: "
                    + str(post_review_result.status_code)
                )
                print("Please report this error to the CI maintainer")
                return 1
        # Wait before posting all chunks so to avoid triggering abuse detection
        time.sleep(5)

    return 0
