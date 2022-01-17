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

from distutils.core import setup

setup(
    name='clang-lint',
    version='0.1dev',
    packages=['clang-lint', 'diff_to_review', 'inline_comments', 'msbuild'],
    license='GNU Public License v3',
    short_description="A Github Action to lint C++ sources with clang tools",
    long_description=open('README').read(),
)
