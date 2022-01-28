#include "stdafx.h"
#include "local_named_pipe.h"
#include <fcntl.h>
namespace Win32Utils
{
    void LocalNamedPipe::closeWriteHandles()
    {
        if (writeFd != -1) {
            // It meands writeFileDescriptor was assigned by means of  _open_osfhandle().
            _close(writeFd);
            writeFd = -1;
        }
        if (hWrite != nullptr) {
            hWrite = nullptr;
        }
    }

    void LocalNamedPipe::openWriteEnd()
    {
        if (hWrite == nullptr && writeFd == -1) {
            hWrite =
              CreateFile(szPipeName.c_str(), GENERIC_WRITE, 0, &writeSA, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
            writeFd = _open_osfhandle(reinterpret_cast<intptr_t>(hWrite), _O_WRONLY | _O_TEXT);
            SetHandleInformation(hWrite, HANDLE_FLAG_INHERIT, static_cast<BOOL>(inheritWrite));
            ConnectNamedPipe(hRead, nullptr); // can only be called when there is a client to connect.
        }
    }
    HANDLE LocalNamedPipe::writeHandle()
    {
        openWriteEnd();
        return hWrite;
    }

    int LocalNamedPipe::writeFileDescriptor()
    {
        openWriteEnd();
        return writeFd;
    }

    LocalNamedPipe::~LocalNamedPipe()
    {
        if (hRead != nullptr) {
            DisconnectNamedPipe(hRead);
            CloseHandle(hRead);
            hRead = nullptr;
        }
        // Notice that hWrite is closed during the console redirection.
        // This should only happen if the console was never redirected but the pipe was created.
        if (hWrite != nullptr) {
            CloseHandle(hWrite);
            hWrite = nullptr;
        }
        if (writeFd != -1) {
            _close(writeFd);
            writeFd = -1;
        }
    }
}
