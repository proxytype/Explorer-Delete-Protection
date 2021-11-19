// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <windows.h>
#include <winternl.h>
#include <iostream>
#include "detours.h"

using namespace std;

#define DELETE                           (0x00010000L)
#define READ_CONTROL                     (0x00020000L)
#define WRITE_DAC                        (0x00040000L)
#define WRITE_OWNER                      (0x00080000L)
#define SYNCHRONIZE                      (0x00100000L)

#define STANDARD_RIGHTS_REQUIRED         (0x000F0000L)

#define STANDARD_RIGHTS_READ             (READ_CONTROL)
#define STANDARD_RIGHTS_WRITE            (READ_CONTROL)
#define STANDARD_RIGHTS_EXECUTE          (READ_CONTROL)

#define STANDARD_RIGHTS_ALL              (0x001F0000L)

#define SPECIFIC_RIGHTS_ALL              (0x0000FFFFL)


typedef BOOL (WINAPI* realDeleteFileW)(
    LPCWSTR lpFileName
);

typedef NTSTATUS (WINAPI*  realNtOpenFile)(
     PHANDLE            FileHandle,
     ACCESS_MASK        DesiredAccess,
     POBJECT_ATTRIBUTES ObjectAttributes,
     PIO_STATUS_BLOCK   IoStatusBlock,
     ULONG              ShareAccess,
     ULONG              OpenOptions
);

typedef BOOL(WINAPI* realMoveFileExW)(
     LPCWSTR lpExistingFileName,
     LPCWSTR lpNewFileName,
     DWORD   dwFlags
);

realNtOpenFile  originalNtOpenFile = (realNtOpenFile)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtOpenFile");
realMoveFileExW originalMoveFileExW = (realMoveFileExW)GetProcAddress(GetModuleHandleA("kernelbase.dll"), "MoveFileExW");
realDeleteFileW originalDeleteFileW = (realDeleteFileW)GetProcAddress(GetModuleHandleA("kernelbase.dll"), "DeleteFileW");


BOOL isProtected(LPCWSTR lpFileName) {

    wstring f(lpFileName);

    size_t found = f.find(L"protected");

    if (found != string::npos) {
        OutputDebugString(L"_DeleteFileW Detectd Protected Folder Requested!");

        return TRUE;
    }
    else {
        return FALSE;

    }
}

BOOL _DeleteFileW(
    LPCWSTR lpFileName
) 
{

    if (isProtected(lpFileName)) {
        OutputDebugString(L"_DeleteFileW(...) Folder Requested!");
        return FALSE;
    }
    else {
        return TRUE;
    }

}


NTSTATUS _ntOpenFile(
    PHANDLE            FileHandle,
    ACCESS_MASK        DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PIO_STATUS_BLOCK   IoStatusBlock,
    ULONG              ShareAccess,
    ULONG              OpenOptions
) {

    if (DesiredAccess == DELETE) {

       
        if (isProtected(ObjectAttributes->ObjectName->Buffer)) {
            OutputDebugString(L"_ntOpenFile(...) Delete Requested!");
            return FALSE;
        }
        else {
            return originalNtOpenFile(FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, ShareAccess, OpenOptions);
        }

    }
    else {
        return originalNtOpenFile(FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, ShareAccess, OpenOptions);
    }
}

BOOL _MoveFileExW(
    LPCWSTR lpExistingFileName,
    LPCWSTR lpNewFileName,
    DWORD   dwFlags
) 
{

    if (isProtected(lpExistingFileName)) {
        OutputDebugString(L"_MoveFileExW(...) Move Requested!");

        return FALSE;
    }
    else {
        return originalMoveFileExW(lpExistingFileName, lpNewFileName, dwFlags);
    }
}


void attachDetour() {

    DetourRestoreAfterWith();
    DetourTransactionBegin();

    DetourUpdateThread(GetCurrentThread());

    DetourAttach((PVOID*)&originalNtOpenFile, _ntOpenFile);
    DetourAttach((PVOID*)&originalDeleteFileW, _DeleteFileW);
    DetourAttach((PVOID*)&originalMoveFileExW, _MoveFileExW);

    DetourTransactionCommit();
}

void deAttachDetour() {

    DetourTransactionBegin();

    DetourUpdateThread(GetCurrentThread());

    DetourDetach((PVOID*)&originalNtOpenFile, _ntOpenFile);
    DetourDetach((PVOID*)&originalMoveFileExW, _MoveFileExW);
    DetourDetach((PVOID*)&originalDeleteFileW, _MoveFileExW);

    DetourTransactionCommit();
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        attachDetour();
        break;
    case DLL_PROCESS_DETACH:
        deAttachDetour();
        break;
    }
    return TRUE;
}

