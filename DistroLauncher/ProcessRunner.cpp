/*
 * Copyright (C) 2021 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"

namespace Helpers {

    ProcessRunner::ProcessRunner(std::wstring_view commandLine) {
        ZeroMemory(&_piProcInfo, sizeof(PROCESS_INFORMATION));
        ZeroMemory(&_siStartInfo, sizeof(STARTUPINFO));
        _sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        _sa.bInheritHandle = TRUE;
        _sa.lpSecurityDescriptor = NULL;
        cmd = commandLine;
        defunct = false;
        alreadyRun = false;
        if (CreatePipe(&g_hChildStd_ERR_Rd, &g_hChildStd_ERR_Wr, &_sa, 0) == 0) {
            setDefunctState();
        }
        if (CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &_sa, 0) == 0) {
            setDefunctState();
        }
        if (!defunct) {
            SetHandleInformation(g_hChildStd_ERR_Rd, HANDLE_FLAG_INHERIT, 0);
            SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0);
            _siStartInfo.cb = sizeof(STARTUPINFO);
            _siStartInfo.hStdError = g_hChildStd_ERR_Wr;
            _siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
            _siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
        }

    }

    bool ProcessRunner::isDefunct() {
        return defunct;
    }

    void ProcessRunner::setDefunctState()
    {
        defunct = true;
        exit_code = ERROR_PROCESS_ABORTED;
    }

    std::wstring_view ProcessRunner::getStdErr() {
        return stdErr;
    }

    std::wstring_view ProcessRunner::getStdOut() {
        return stdOut;
    }

    DWORD ProcessRunner::getExitCode() {
        return exit_code;
    }

    DWORD ProcessRunner::run() {
        if (alreadyRun || defunct) {
            return exit_code;
        }

        TCHAR szCmdline[80];
        wcsncpy_s(szCmdline, cmd.data(), cmd.length());
        exit_code = CreateProcess(NULL,     // command line 
            szCmdline,                      // non-const CLI
            NULL,                           // process security attributes 
            NULL,                           // primary thread security attributes 
            TRUE,                           // handles are inherited 
            0,                              // creation flags 
            NULL,                           // use parent's environment 
            NULL,                           // use parent's current directory 
            &_siStartInfo,                  // STARTUPINFO pointer 
            &_piProcInfo);                  // output: PROCESS_INFORMATION 
        CloseHandle(g_hChildStd_ERR_Wr);
        CloseHandle(g_hChildStd_OUT_Wr);
        if (exit_code == 0) {
            exit_code = ERROR_CREATE_FAILED;
            return exit_code;
        }

        read_pipes();
        WaitForSingleObject(_piProcInfo.hProcess, 1000);
        GetExitCodeProcess(_piProcInfo.hProcess, &exit_code);
        alreadyRun = true;
        return exit_code;
    }

    ProcessRunner::~ProcessRunner() {
        if (!defunct) {
            CloseHandle(g_hChildStd_OUT_Rd);
            CloseHandle(g_hChildStd_ERR_Rd);
            CloseHandle(_piProcInfo.hProcess);
            CloseHandle(_piProcInfo.hThread);
        }
    }

    void ProcessRunner::read_pipes() {
        DWORD dwRead;
        const size_t BUFSIZE = 80;
        TCHAR chBuf[BUFSIZE];
        bool bSuccess = FALSE;
        for (;;) {
            bSuccess = ReadFile(g_hChildStd_OUT_Rd, chBuf, BUFSIZE, &dwRead, NULL);
            if (!bSuccess || dwRead == 0) {
                break;
            }

            stdOut.append(chBuf, dwRead);
        }

        dwRead = 0;
        for (;;) {
            bSuccess = ReadFile(g_hChildStd_ERR_Rd, chBuf, BUFSIZE, &dwRead, NULL);
            if (!bSuccess || dwRead == 0) {
                break;
            }

            stdErr.append(chBuf, dwRead);
        }
    }
}