#!/bin/bash
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

# Runs the clang_lint.__main__.py main() function.
# That acts as an aggregator of pull request review comments generated
# by a pipeline of linters and formatters (conceptually), here implemented
# by clang-format and clang-tidy, only.
# Clang-format is run in this pipeline, but clang-tidy is expected to have been
# already run. That's for maximum portability, since I aim to make
# this pipeline cross-platform.
# Running clang-tidy on Windows might be tricky if the project doesn't build
# with CMake and requires full command line customization, beyond what is
# offered by MSBuild.


set -eu

# Find the pull request id.
if [ -z "$pull_request_id" ]; then
    pull_request_id="$(jq "if (.issue.number != null) then .issue.number else .number end" < "$GITHUB_EVENT_PATH")"
fi

if [ "$pull_request_id" == "null" ]; then
echo "Could not find the pull request ID. Is this a pull request?"
exit 0
fi

cd .github/workflows
pip3 install -r clang_lint/requirements.txt

python3 -m clang_lint --repository-root "$GITHUB_WORKSPACE" \
    --repository-name "$GITHUB_REPOSITORY"                  \
    --pull-request-id "$pull_request_id"                    \
    --run-clang-format                                      \
    --fallback-style "Webkit"                               \
    --build-dir "build"                                     \
    --run-clang-tidy                                        \

