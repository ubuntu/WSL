@echo off
REM
REM Copyright (C) 2021 Canonical Ltd
REM
REM This program is free software: you can redistribute it and/or modify
REM it under the terms of the GNU General Public License version 3 as
REM published by the Free Software Foundation.
REM
REM This program is distributed in the hope that it will be useful,
REM but WITHOUT ANY WARRANTY; without even the implied warranty of
REM MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
REM GNU General Public License for more details.
REM
REM You should have received a copy of the GNU General Public License
REM along with this program.  If not, see <http://www.gnu.org/licenses/>.
REM
REM
REM Since this file will run as if it was clang-tidy, it's working directory will be
REM the project folder, which for WSL is right below the root directory.
@echo on

SET PYTHONPATH=../.github/workflows/
python3 -m clang_lint.msbuild  %*