#include "buildcfg.h"
#include "kxuserp.h"
#include <ShlObj.h>
#pragma comment(lib, "shell32.lib")

// Vista has this API, but not the two per-user Program Files folder IDs.
// Preserve native handling for every other folder and for successful queries.
KXUSERAPI HRESULT WINAPI Ext_SHGetKnownFolderPath(
    const GUID *Folder, DWORD Flags, HANDLE Token, PWSTR *Path)
{
    static const GUID Programs = {0x5cd7aee2,0x2219,0x4a67,{0xb8,0x5d,0x6c,0x9c,0xe1,0x56,0x60,0xcb}};
    static const GUID Common = {0xbcbd3057,0xca5c,0x4622,{0xb4,0x2d,0xbc,0x56,0xdb,0x0a,0xe5,0x16}};
    static const GUID Local = {0xf1b32785,0x6fba,0x4fcf,{0x9d,0x55,0x7b,0x8e,0x7f,0x15,0x70,0x91}};
    HRESULT Status;
    PWSTR Base = NULL, Result;
    PCWSTR Suffix;
    SIZE_T Count;
    DWORD Attributes, Error;
    if (!Path || !Folder) return E_INVALIDARG;
    *Path = NULL;
    Status = SHGetKnownFolderPath(Folder, Flags, Token, Path);
    if (SUCCEEDED(Status)) return Status;
    if (memcmp(Folder,&Programs,sizeof(GUID)) == 0) Suffix=L"\\Programs";
    else if (memcmp(Folder,&Common,sizeof(GUID)) == 0) Suffix=L"\\Programs\\Common";
    else return Status;
    if (Status != E_INVALIDARG && Status != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) return Status;
    CoTaskMemFree(*Path);
    *Path = NULL;
    // Let the native API resolve the requested user's LocalAppData and flags.
    Status = SHGetKnownFolderPath(&Local, Flags, Token, &Base);
    if (FAILED(Status)) { CoTaskMemFree(Base); return Status; }
    Count = wcslen(Base) + wcslen(Suffix) + 1;
    Result = CoTaskMemAlloc(Count * sizeof(WCHAR));
    if (!Result) { CoTaskMemFree(Base); return E_OUTOFMEMORY; }
    StringCchCopyW(Result,Count,Base);
    StringCchCatW(Result,Count,Suffix);
    CoTaskMemFree(Base);
    if (Flags & KF_FLAG_CREATE) {
        Error = SHCreateDirectoryExW(NULL,Result,NULL);
        if (Error != ERROR_SUCCESS && Error != ERROR_ALREADY_EXISTS && Error != ERROR_FILE_EXISTS) {
            CoTaskMemFree(Result); return HRESULT_FROM_WIN32(Error);
        }
    }
    if (!(Flags & KF_FLAG_DONT_VERIFY) || (Flags & KF_FLAG_CREATE)) {
        Attributes = GetFileAttributesW(Result);
        if (Attributes == INVALID_FILE_ATTRIBUTES || !(Attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            Error = Attributes == INVALID_FILE_ATTRIBUTES ? GetLastError() : ERROR_DIRECTORY;
            CoTaskMemFree(Result); return HRESULT_FROM_WIN32(Error);
        }
    }
    *Path = Result;
    return S_OK;
}
