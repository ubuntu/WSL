#!/usr/bin/python3
""" Very simple markdown escaping function to avoid text to be interpreted
as markdown syntax. """
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

import re


def markdown(s):
    """ Escapes markdown special characters or unescape the ones
    improperly escaped in block quotes. """
    md_chars = "\\`*_{}[]<>()#+-.!|"

    def escape_chars(s):
        for ch in md_chars:
            s = s.replace(ch, "\\" + ch)

        return s

    def unescape_chars(s):
        for ch in md_chars:
            s = s.replace("\\" + ch, ch)

        return s

    # Escape markdown characters
    s = escape_chars(s)
    # Decorate quoted symbols as code
    s = re.sub(
        "'([^']*)'",
        lambda match:
            "`` {} ``".format(unescape_chars(match.group(1))),
        s
    )

    return s
