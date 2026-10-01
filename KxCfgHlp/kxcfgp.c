///////////////////////////////////////////////////////////////////////////////
//
// Module Name:
//
//     kxcfgp.c
//
// Abstract:
//
//     Utility functions for KxCfgHlp.
//
// Author:
//
//     vxiiduu (03-Feb-2024)
//
// Environment:
//
//     Win32 mode. This code must be able to run without KexDll, as it is used
//     in KexSetup. This code must function properly when run under WOW64.
//
// Revision History:
//
//     vxiiduu              03-Feb-2024  Initial creation.
//
///////////////////////////////////////////////////////////////////////////////

#include "buildcfg.h"
#include <KxCfgHlp.h>
#include <KexW32ML.h>

INT NTAPI RtlOperatingSystemBitness(VOID);

//
// This function removes KexDll.dll from a space-separated list (same format as
// you'd find in the IFEO VerifierDlls value).
//
// Returns TRUE if KexDll.dll was removed, FALSE if it was not found.
// If KexDll.dll was the only verifier DLL in the list, then this function will
// cause VerifierDlls to be an empty string.
//
// Matching is case-insensitive and requires a complete DLL token. Other DLL
// names, casing and whitespace are preserved. All KexDll tokens are removed.
//
// For reference: The function within NTDLL that parses the VerifierDlls list is
// called AVrfpParseVerifierDllsString. It is tolerant of double-spacing.
//
BOOLEAN KxCfgpRemoveKexDllFromVerifierDlls(IN PWSTR VerifierDlls)
{
    PWSTR Read, Write;
    BOOLEAN Removed = FALSE, HasOtherDll = FALSE;
    ASSERT (VerifierDlls != NULL);
    if (!VerifierDlls) return FALSE;
    Read = Write = VerifierDlls;
    while (*Read) {
        PCWSTR Token;
        SIZE_T Length;
        if (*Read == L' ' || *Read == L'\t') {
            *Write++ = *Read++;
            continue;
        }
        Token = Read;
        while (*Read && *Read != L' ' && *Read != L'\t') ++Read;
        Length = Read - Token;
        if (Length == StringLiteralLength(L"kexdll.dll") &&
            !_wcsnicmp(Token, L"kexdll.dll", Length)) {
            Removed = TRUE;
            continue;
        }
        HasOtherDll = TRUE;
        while (Length--) *Write++ = *Token++;
    }
    *Write = L'\0';
    if (Removed && !HasOtherDll) VerifierDlls[0] = L'\0';
    return Removed;
}
// Vista redirects IFEO for WOW64; select the target image's registry view.
STATIC BOOLEAN KxCfgpRecordedPathMatches(PCWSTR ExeFullPath, REGSAM View)
{
    HKEY Base, Key;
    LONG Error;
    WCHAR Recorded[MAX_PATH] = {0};
    BOOLEAN Match = FALSE;
    Error = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options",
        0, KEY_READ | View, &Base);
    if (Error) return FALSE;
    Error = RegOpenKeyExW(Base, PathFindFileName(ExeFullPath), 0, KEY_READ | View, &Key);
    RegCloseKey(Base);
    if (Error) return FALSE;
    Error = RegReadString(Key, NULL, L"KEX_ConfigPath", Recorded, ARRAYSIZE(Recorded));
    if (!Error) Match = StringEqualI(Recorded, ExeFullPath);
    RegCloseKey(Key);
    return Match;
}

REGSAM KxCfgpIfeoView(PCWSTR ExeFullPath)
{
    OSVERSIONINFOW Version = {sizeof(Version)};
    DWORD BinaryType;
    RtlGetVersion(&Version);
    if (Version.dwMajorVersion == 6 && Version.dwMinorVersion == 0) {
        if (GetBinaryTypeW(ExeFullPath, &BinaryType)) {
            return BinaryType == SCS_32BIT_BINARY ? KEY_WOW64_32KEY : KEY_WOW64_64KEY;
        }
        if (RtlOperatingSystemBitness() != 64) return KEY_WOW64_64KEY;
        // A missing image cannot reveal its bitness. Match recorded full paths,
        // never just a basename in the alternate view.
        if (KxCfgpRecordedPathMatches(ExeFullPath, KEY_WOW64_32KEY)) {
            if (KxCfgpRecordedPathMatches(ExeFullPath, KEY_WOW64_64KEY)) {
                SetLastError(ERROR_DUP_NAME);
                return 0; // Ambiguous: callers must not mutate either view.
            }
            return KEY_WOW64_32KEY;
        }
    }
    return KEY_WOW64_64KEY;
}

NTSTATUS KxCfgpOpenIfeoKey(PCWSTR ExeFullPath, PHKEY KeyHandle)
{
    UNICODE_STRING Name;
    HKEY Base;
    LONG Error;
    OSVERSIONINFOW Version = {sizeof(Version)};
    REGSAM View = KxCfgpIfeoView(ExeFullPath);
    if (!View) return STATUS_OBJECT_NAME_COLLISION;
    RtlGetVersion(&Version);
    if (Version.dwMajorVersion != 6 || Version.dwMinorVersion != 0) {
        RtlInitUnicodeString(&Name, ExeFullPath);
        return LdrOpenImageFileOptionsKey(&Name, FALSE, (PHANDLE)KeyHandle);
    }
    Error = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options",
        0, KEY_READ | View, &Base);
    if (Error == ERROR_SUCCESS) {
        Error = RegOpenKeyExW(Base, PathFindFileName(ExeFullPath), 0,
            KEY_READ | View, KeyHandle);
        RegCloseKey(Base);
    }
    if (Error == ERROR_SUCCESS) return STATUS_SUCCESS;
    if (Error == ERROR_FILE_NOT_FOUND) return STATUS_OBJECT_NAME_NOT_FOUND;
    return STATUS_ACCESS_DENIED;
}

BOOLEAN KxCfgpCreateIfeoKeyForProgram(
	IN	PCWSTR	ExeFullPath,
	OUT	PHKEY	KeyHandle,
	IN	HANDLE	TransactionHandle OPTIONAL)
{
	ULONG ErrorCode;
	PCWSTR ExeBaseName;
	HKEY IfeoBaseKey;
	HKEY IfeoExeKey;

	ASSERT (ExeFullPath != NULL);
	ASSERT (ExeFullPath[0] != '\0');
	ASSERT (KeyHandle != NULL);

	*KeyHandle = NULL;
	if (!KxCfgpIfeoView(ExeFullPath)) return FALSE;

	IfeoBaseKey = NULL;
	IfeoExeKey = NULL;

	//
	// Open the IFEO base key.
	//

	// Note: Since we are opening the base key handle, we do not need to,
	// and in fact should not close it after we are done using it.
	// See ntdll!RtlOpenImageFileOptionsKey for more information.
	
	ErrorCode = RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		L"Software\\Microsoft\\Windows NT\\CurrentVersion\\"
		L"Image File Execution Options",
		0,
		KEY_READ | KEY_WRITE | KxCfgpIfeoView(ExeFullPath),
		&IfeoBaseKey);

	if (ErrorCode == ERROR_FILE_NOT_FOUND) {
		ErrorCode = RegCreateKeyEx(
			HKEY_LOCAL_MACHINE,
			L"Software\\Microsoft\\Windows NT\\CurrentVersion\\"
			L"Image File Execution Options",
			0,
			NULL,
			0,
			KEY_READ | KEY_WRITE | KxCfgpIfeoView(ExeFullPath),
			NULL,
			&IfeoBaseKey,
			NULL);
		ASSERT (ErrorCode == ERROR_SUCCESS);
	} else if (ErrorCode != ERROR_SUCCESS) {
		SetLastError(ErrorCode);
		return FALSE;
	}

	ASSERT (ErrorCode == ERROR_SUCCESS);

	ExeBaseName = PathFindFileName(ExeFullPath);
	if (ExeBaseName == NULL) {
		// caller must have passed some garbage path...
		RegCloseKey(IfeoBaseKey);
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	//
	// Create the EXE key for the program.
	//

	if (TransactionHandle) {
		ErrorCode = RegCreateKeyTransacted(
			IfeoBaseKey,
			ExeBaseName,
			0,
			NULL,
			0,
			KEY_READ | KEY_WRITE | KxCfgpIfeoView(ExeFullPath),
			NULL,
			&IfeoExeKey,
			NULL,
			TransactionHandle,
			NULL);
	} else {
		ErrorCode = RegCreateKeyEx(
			IfeoBaseKey,
			ExeBaseName,
			0,
			NULL,
			0,
			KEY_READ | KEY_WRITE | KxCfgpIfeoView(ExeFullPath),
			NULL,
			&IfeoExeKey,
			NULL);
	}

	if (ErrorCode != ERROR_SUCCESS) {
		RegCloseKey(IfeoBaseKey);
		SetLastError(ErrorCode);
		return FALSE;
	}

	*KeyHandle = IfeoExeKey;
	RegCloseKey(IfeoBaseKey);
	return TRUE;
}

HKEY KxCfgpCreateKey(
	IN	HKEY		RootDirectory,
	IN	PCWSTR		KeyPath,
	IN	ACCESS_MASK	DesiredAccess,
	IN	HANDLE		TransactionHandle OPTIONAL)
{
	HKEY KeyHandle;
	ULONG ErrorCode;

	// 64-bit key is the default, no need to specify.
	ASSERT (!(DesiredAccess & KEY_WOW64_64KEY));

	unless (DesiredAccess & KEY_WOW64_32KEY) {
		DesiredAccess |= KEY_WOW64_64KEY;
	}

	if (TransactionHandle) {
		ErrorCode = RegCreateKeyTransacted(
			RootDirectory,
			KeyPath,
			0,
			NULL,
			0,
			DesiredAccess,
			NULL,
			&KeyHandle,
			NULL,
			TransactionHandle,
			NULL);
	} else {
		ErrorCode = RegCreateKeyEx(
			RootDirectory,
			KeyPath,
			0,
			NULL,
			0,
			DesiredAccess,
			NULL,
			&KeyHandle,
			NULL);
	}

	if (ErrorCode != ERROR_SUCCESS) {
		SetLastError(ErrorCode);
		return NULL;
	}

	return KeyHandle;
}

HKEY KxCfgpOpenKey(
	IN	HKEY		RootDirectory,
	IN	PCWSTR		KeyPath,
	IN	ACCESS_MASK	DesiredAccess,
	IN	HANDLE		TransactionHandle OPTIONAL)
{
	HKEY KeyHandle;
	ULONG ErrorCode;

	// 64-bit key is the default, no need to specify.
	ASSERT (!(DesiredAccess & KEY_WOW64_64KEY));

	unless (DesiredAccess & KEY_WOW64_32KEY) {
		DesiredAccess |= KEY_WOW64_64KEY;
	}

	if (TransactionHandle) {
		ErrorCode = RegOpenKeyTransacted(
			RootDirectory,
			KeyPath,
			0,
			DesiredAccess,
			&KeyHandle,
			TransactionHandle,
			NULL);
	} else {
		ErrorCode = RegOpenKeyEx(
			RootDirectory,
			KeyPath,
			0,
			DesiredAccess,
			&KeyHandle);
	}

	if (ErrorCode != ERROR_SUCCESS) {
		SetLastError(ErrorCode);
		return NULL;
	}

	return KeyHandle;
}

//
// WARNING: This API is unlike the others since it takes a NT style registry path
// and doesn't accept predefined handles. Instead of HKEY_LOCAL_MACHINE, prefix KeyPath
// with \Registry\Machine. Instead of HKEY_CURRENT_USER, use RtlOpenCurrentUser.
//

ULONG KxCfgpDeleteKey(
	IN	HKEY	KeyHandle OPTIONAL,
	IN	PCWSTR	KeyPath OPTIONAL,
	IN	HANDLE	TransactionHandle OPTIONAL)
{
	NTSTATUS Status;
	OBJECT_ATTRIBUTES ObjectAttributes;
	UNICODE_STRING KeyPathUS;

	if (KeyPath || TransactionHandle) {
		//
		// We need to either re-open the root key transacted or open the key path
		// so that we can delete it with NtDeleteKey.
		//

		if (KeyPath) {
			Status = RtlInitUnicodeStringEx(&KeyPathUS, KeyPath);
			ASSERT (NT_SUCCESS(Status));

			if (!NT_SUCCESS(Status)) {
				return RtlNtStatusToDosError(Status);
			}

			InitializeObjectAttributes(
				&ObjectAttributes,
				&KeyPathUS,
				OBJ_CASE_INSENSITIVE,
				KeyHandle,
				NULL);
		} else {
			InitializeObjectAttributes(
				&ObjectAttributes,
				NULL,
				0,
				KeyHandle,
				NULL);
		}

		if (TransactionHandle) {
			Status = NtOpenKeyTransacted(
				(PHANDLE) &KeyHandle,
				DELETE | KEY_WOW64_64KEY,
				&ObjectAttributes,
				TransactionHandle);
		} else {
			Status = NtOpenKey(
				(PHANDLE) &KeyHandle,
				DELETE | KEY_WOW64_64KEY,
				&ObjectAttributes);
		}

		if (Status == STATUS_OBJECT_NAME_NOT_FOUND) {
			return ERROR_SUCCESS;
		}

		ASSERT (NT_SUCCESS(Status));

		if (!NT_SUCCESS(Status)) {
			return RtlNtStatusToDosError(Status);
		}

		Status = NtDeleteKey(KeyHandle);
		SafeClose(KeyHandle);
	} else {
		Status = NtDeleteKey(KeyHandle);
	}

	ASSERT (NT_SUCCESS(Status));
	return RtlNtStatusToDosError(Status);
}
