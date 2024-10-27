#include <stdafx.h>
#include "file_system.h"
#include <kernel/xam.h>
#include <kernel/xdm.h>
#include <kernel/function.h>
#include <cpu/guest_thread.h>

bool FindHandleCloser(void* handle)
{
    FindClose(handle);
    return false;
}

SWA_API uint32_t XCreateFileA(
    LPCSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes)
{
    const auto handle = (uint32_t)CreateFileA(
        FileSystem::TransformPath(lpFileName),
        dwDesiredAccess,
        dwShareMode,
        nullptr,
        dwCreationDisposition,
        dwFlagsAndAttributes & ~(FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED),
        nullptr);

    GuestThread::SetLastError(GetLastError());
    printf("CreateFileA(%s, %lx, %lx, %p, %lx, %lx): %x\n", lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, handle);

    return handle;
}

static DWORD XGetFileSizeA(
    uint32_t hFile,
    LPDWORD lpFileSizeHigh)
{
    DWORD fileSize = GetFileSize((HANDLE)hFile, lpFileSizeHigh);
    if (lpFileSizeHigh != nullptr)
        *lpFileSizeHigh = std::byteswap(*lpFileSizeHigh);

    return fileSize;
}

BOOL XGetFileSizeExA(
    uint32_t hFile,
    PLARGE_INTEGER lpFileSize)
{
    BOOL result = GetFileSizeEx((HANDLE)hFile, lpFileSize);
    if (result)
        lpFileSize->QuadPart = std::byteswap(lpFileSize->QuadPart);

    return result;
}

BOOL XReadFile(
    uint32_t hFile,
    LPVOID lpBuffer,
    DWORD nNumberOfBytesToRead,
    XLPDWORD lpNumberOfBytesRead,
    XOVERLAPPED* lpOverlapped)
{
    if (lpOverlapped != nullptr)
    {
        LONG distanceToMoveHigh = lpOverlapped->OffsetHigh;
        if (SetFilePointer((HANDLE)hFile, lpOverlapped->Offset, &distanceToMoveHigh, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
            return FALSE;
    }

    DWORD numberOfBytesRead;
    BOOL result = ReadFile((HANDLE)hFile, lpBuffer, nNumberOfBytesToRead, &numberOfBytesRead, nullptr);
    if (result)
    {
        if (lpOverlapped != nullptr)
        {
            lpOverlapped->Internal = 0;
            lpOverlapped->InternalHigh = numberOfBytesRead;

            if (lpOverlapped->hEvent != NULL)
                SetEvent((HANDLE)lpOverlapped->hEvent.get());
        }
        else if (lpNumberOfBytesRead != nullptr)
        {
            *lpNumberOfBytesRead = numberOfBytesRead;
        }
    }

    //printf("ReadFile(): %x %x %x %x %x %x\n", hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped, result);

    return result;
}

DWORD XSetFilePointer(
    uint32_t hFile,
    LONG lDistanceToMove,
    PLONG lpDistanceToMoveHigh,
    DWORD dwMoveMethod)
{
    LONG distanceToMoveHigh = lpDistanceToMoveHigh ? std::byteswap(*lpDistanceToMoveHigh) : 0;
    DWORD result = SetFilePointer((HANDLE)hFile, lDistanceToMove, lpDistanceToMoveHigh ? &distanceToMoveHigh : nullptr, dwMoveMethod);
    if (lpDistanceToMoveHigh != nullptr)
        *lpDistanceToMoveHigh = std::byteswap(distanceToMoveHigh);

    return result;
}

BOOL XSetFilePointerEx(
    uint32_t hFile,
    LONG lDistanceToMove,
    PLARGE_INTEGER lpNewFilePointer,
    DWORD dwMoveMethod)
{
    LARGE_INTEGER distanceToMove;
    distanceToMove.QuadPart = lDistanceToMove;

    DWORD result = SetFilePointerEx((HANDLE)hFile, distanceToMove, lpNewFilePointer, dwMoveMethod);
    if (lpNewFilePointer != nullptr)
        lpNewFilePointer->QuadPart = std::byteswap(lpNewFilePointer->QuadPart);

    return result;
}

uint32_t XFindFirstFileA(
    LPCSTR lpFileName,
    LPWIN32_FIND_DATAA lpFindFileData)
{
    auto& data = *lpFindFileData;
    const auto handle = FindFirstFileA(FileSystem::TransformPath(lpFileName), &data);
    GuestThread::SetLastError(GetLastError());
    if (handle == INVALID_HANDLE_VALUE)
    {
        return 0xFFFFFFFF;
    }

    ByteSwap(data.dwFileAttributes);
    ByteSwap(*(uint64_t*)&data.ftCreationTime);
    ByteSwap(*(uint64_t*)&data.ftLastAccessTime);
    ByteSwap(*(uint64_t*)&data.ftLastWriteTime);
    ByteSwap(*(uint64_t*)&data.nFileSizeHigh);

    return GUEST_HANDLE(ObInsertObject(handle, FindHandleCloser));
}

uint32_t XFindNextFileA(uint32_t Handle, LPWIN32_FIND_DATAA lpFindFileData)
{
    auto* handle = ObQueryObject(HOST_HANDLE(Handle));
    auto& data = *lpFindFileData;
    const auto result = FindNextFileA(handle, &data);

    ByteSwap(data.dwFileAttributes);
    ByteSwap(*(uint64_t*)&data.ftCreationTime);
    ByteSwap(*(uint64_t*)&data.ftLastAccessTime);
    ByteSwap(*(uint64_t*)&data.ftLastWriteTime);
    ByteSwap(*(uint64_t*)&data.nFileSizeHigh);

    return result;
}

BOOL XReadFileEx(
    uint32_t hFile,
    LPVOID lpBuffer,
    DWORD nNumberOfBytesToRead,
    XOVERLAPPED* lpOverlapped,
    uint32_t lpCompletionRoutine)
{
    LONG distanceToMoveHigh = lpOverlapped->OffsetHigh;
    if (SetFilePointer((HANDLE)hFile, lpOverlapped->Offset, &distanceToMoveHigh, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
        return FALSE;

    DWORD numberOfBytesRead;
    BOOL result = ReadFile((HANDLE)hFile, lpBuffer, nNumberOfBytesToRead, &numberOfBytesRead, nullptr);
    if (result)
    {
        lpOverlapped->Internal = 0;
        lpOverlapped->InternalHigh = numberOfBytesRead;

        if (lpOverlapped->hEvent != NULL)
            SetEvent((HANDLE)lpOverlapped->hEvent.get());
    }

    //printf("ReadFileEx(): %x %x %x %x %x %x\n", hFile, lpBuffer, nNumberOfBytesToRead, lpOverlapped, lpCompletionRoutine, result);

    return result;
}

DWORD XGetFileAttributesA(LPCSTR lpFileName)
{
    return GetFileAttributesA(FileSystem::TransformPath(lpFileName));
}

BOOL XWriteFile(
    uint32_t hFile,
    LPCVOID lpBuffer,
    DWORD nNumberOfBytesToWrite,
    LPDWORD lpNumberOfBytesWritten,
    LPOVERLAPPED lpOverlapped)
{
    assert(lpOverlapped == nullptr);

    BOOL result = WriteFile((HANDLE)hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, nullptr);
    if (result && lpNumberOfBytesWritten != nullptr)
        ByteSwap(*lpNumberOfBytesWritten);

    return result;
}

const char* FileSystem::TransformPath(const char* path)
{
    thread_local char builtPath[2048]{};
    const char* relativePath = strstr(path, ":\\");
    if (relativePath != nullptr)
    {
        // rooted folder, handle direction
        const std::string_view root = std::string_view{ path, path + (relativePath - path) };
        const auto newRoot = XamGetRootPath(root);

        if (!newRoot.empty())
        {
            strncpy(builtPath, newRoot.data(), newRoot.size());
            builtPath[newRoot.size()] = '\\';
            strcpy(builtPath + newRoot.size() + 1, relativePath + 2);

            return builtPath;
        }
    }

    return relativePath != nullptr ? relativePath + 2 : path;
}

SWA_API const char* XExpandFilePathA(const char* path)
{
    return FileSystem::TransformPath(path);
}

GUEST_FUNCTION_HOOK(sub_82BD4668, XCreateFileA);
GUEST_FUNCTION_HOOK(sub_82BD4600, XGetFileSizeA);
GUEST_FUNCTION_HOOK(sub_82BD5608, XGetFileSizeExA);
GUEST_FUNCTION_HOOK(sub_82BD4478, XReadFile);
GUEST_FUNCTION_HOOK(sub_831CD3E8, XSetFilePointer);
GUEST_FUNCTION_HOOK(sub_831CE888, XSetFilePointerEx);
GUEST_FUNCTION_HOOK(sub_831CDC58, XFindFirstFileA);
GUEST_FUNCTION_HOOK(sub_831CDC00, XFindNextFileA);
GUEST_FUNCTION_HOOK(sub_831CDF40, XReadFileEx);
GUEST_FUNCTION_HOOK(sub_831CD6E8, XGetFileAttributesA);
GUEST_FUNCTION_HOOK(sub_831CE3F8, XCreateFileA);
GUEST_FUNCTION_HOOK(sub_82BD4860, XWriteFile);
