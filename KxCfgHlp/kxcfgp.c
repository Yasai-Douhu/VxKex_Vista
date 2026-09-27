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

//
// This function removes KexDll.dll from a space-separated list (same format as
// you'd find in the IFEO VerifierDlls value).
//
// Returns TRUE if KexDll.dll was removed, FALSE if it was not found.
// If KexDll.dll was the only verifier DLL in the list, then this function will
// cause VerifierDlls to be an empty string.
//
// As a side effect, VerifierDlls will be lowercased. This does not affect the
// normal operation of Application Verifier, since their verifier DLLs are all
// lower case anyway.
//
// For reference: The function within NTDLL that parses the VerifierDlls list is
// called AVrfpParseVerifierDllsString. It is tolerant of double-spacing.
//
BOOLEAN KxCfgpRemoveKexDllFromVerifierDlls(
	IN	PWSTR	VerifierDlls)
{
	ULONG Index;
	PWSTR KexDll;
	PCWSTR AfterKexDll;
	BOOLEAN NonSeparatorCharacterFound;

	ASSERT (VerifierDlls != NULL);

	KexDll = (PWSTR) StringFindI(VerifierDlls, L"kexdll.dll");

	if (!KexDll) {
		// KexDll.dll was not found in the verifier DLLs list.
		return FALSE;
	}

	//
	// Shift backwards the contents of the string in order to remove the
	// "kexdll.dll" entry.
	//

	AfterKexDll = KexDll + StringLiteralLength(L"kexdll.dll");
	Index = 0;

	do {
		KexDll[Index] = AfterKexDll[Index];
	} until (AfterKexDll[Index++] == '\0');

	//
	// Check to see if the VerifierDlls string consists of only separators.
	// (note: AVrfpParseVerifierDllsString considers ' ' and '\t' to be separators)
	//

	Index = 0;
	NonSeparatorCharacterFound = FALSE;

	until (VerifierDlls[Index] == '\0') {
		if (VerifierDlls[Index] != ' ' && VerifierDlls[Index] != '\t') {
			NonSeparatorCharacterFound = TRUE;
			break;
		}

		++Index;
	}

	if (NonSeparatorCharacterFound == FALSE) {
		// The whole thing is just whitespace.
		// Overwrite it so that it becomes an empty string.
		VerifierDlls[0] = '\0';
	}

	return TRUE;
}

// Vista redirects IFEO for WOW64; select the target image's registry view.
REGSAM KxCfgpIfeoView(PCWSTR ExeFullPath)
{
    OSVERSIONINFOW Version = {sizeof(Version)};
    DWORD BinaryType;
    RtlGetVersion(&Version);
    if (Version.dwMajorVersion == 6 && Version.dwMinorVersion == 0 &&
        GetBinaryTypeW(ExeFullPath, &BinaryType) && BinaryType == SCS_32BIT_BINARY)
        return KEY_WOW64_32KEY;
    return KEY_WOW64_64KEY;
}

NTSTATUS KxCfgpOpenIfeoKey(PCWSTR ExeFullPath, PHKEY KeyHandle)
{
    UNICODE_STRING Name;
    HKEY Base;
    LONG Error;
    if (KxCfgpIfeoView(ExeFullPath) != KEY_WOW64_32KEY) {
        RtlInitUnicodeString(&Name, ExeFullPath);
        return LdrOpenImageFileOptionsKey(&Name, FALSE, (PHANDLE)KeyHandle);
    }
    Error = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options",
        0, KEY_READ | KEY_WOW64_32KEY, &Base);
    if (Error == ERROR_SUCCESS) {
        Error = RegOpenKeyExW(Base, PathFindFileName(ExeFullPath), 0,
            KEY_READ | KEY_WOW64_32KEY, KeyHandle);
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