///////////////////////////////////////////////////////////////////////////////
//
// Module Name:
//
//     stubs.c
//
// Abstract:
//
//     Forwarder stubs that do nothing except for calling the original function.
//     These exist because some stupid software such as Chromium is not
//     compatible with export forwarders.
//
// Author:
//
//     vxiiduu (09-Mar-2024)
//
// Environment:
//
//     Win32
//
// Revision History:
//
//     vxiiduu              09-Mar-2024  Initial creation.
//
///////////////////////////////////////////////////////////////////////////////

#include "buildcfg.h"
#include "kxbasep.h"

KXBASEAPI BOOL WINAPI Stub_DuplicateHandle(
	IN	HANDLE		SourceProcessHandle,
	IN	HANDLE		SourceHandle,
	IN	HANDLE		TargetProcessHandle,
	OUT	PHANDLE		TargetHandle,
	IN	ACCESS_MASK	DesiredAccess,
	IN	BOOL		Inherit,
	IN	ULONG		Options)
{
	return DuplicateHandle(
		SourceProcessHandle,
		SourceHandle,
		TargetProcessHandle,
		TargetHandle,
		DesiredAccess,
		Inherit,
		Options);
}

KXBASEAPI FARPROC WINAPI Stub_GetProcAddress(
	IN	HMODULE		ModuleHandle,
	IN	PCSTR		ProcedureName)
{
	// Inno's SafeDLLPath can obtain the native kernel32 handle on Vista.
	// Route this dynamic lookup through the same profile as a static import.
	if (OriginalMajorVersion == 6 && OriginalMinorVersion == 0 && KexData &&
		(KexData->Flags & KEXDATA_FLAG_INNO_SETUP) &&
		!KexData->IfeoParameters.DisableAppSpecific &&
		ModuleHandle && ModuleHandle == KexData->BaseDllBase &&
		(ULONG_PTR) ProcedureName > 0xffff &&
		!strcmp(ProcedureName, "SetDefaultDllDirectories")) {
		return (FARPROC) Ext_SetDefaultDllDirectories;
	}

	// Vista exposes this API, but lacks the per-user Programs folder IDs.
	// Some applications obtain the native shell handle without DLL rewriting.
	if (OriginalMajorVersion == 6 && OriginalMinorVersion == 0 &&
		(ULONG_PTR) ProcedureName > 0xffff &&
		!strcmp(ProcedureName, "SHGetKnownFolderPath") &&
		ModuleHandle && ModuleHandle == GetModuleHandleW(L"shell32.dll") && KexData) {
		WCHAR Path[MAX_PATH];
		HMODULE UserModule;
		FARPROC Procedure;
		DWORD SavedError = GetLastError();
		if (SUCCEEDED(StringCchCopyW(Path, ARRAYSIZE(Path), KexData->Kex3264DirPath.Buffer)) &&
			SUCCEEDED(StringCchCatW(Path, ARRAYSIZE(Path), L"\\KxUser.dll"))) {
			UserModule = GetModuleHandleW(Path);
			if (!UserModule) UserModule = LoadLibraryW(Path);
			if (UserModule) {
				Procedure = GetProcAddress(UserModule, ProcedureName);
				if (Procedure) {
					SetLastError(SavedError);
					return Procedure;
				}
			}
		}
		SetLastError(SavedError);
	}
	return GetProcAddress(
		ModuleHandle,
		ProcedureName);
}
