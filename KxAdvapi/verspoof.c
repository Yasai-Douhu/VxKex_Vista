#include "buildcfg.h"
#include <KexComm.h>
#include <KexDll.h>

// Strong registry spoofing is per-process and read-only. Never modify HKLM.
STATIC BOOLEAN KxVersionRegistryValue(
	HKEY Key, PCWSTR Name, PVOID Caller, PULONG Type,
	PULONG Number, PCWSTR *Text)
{
	PKEX_PROCESS_DATA Data;
	BYTE Buffer[1024];
	PKEY_NAME_INFORMATION Info = (PKEY_NAME_INFORMATION) Buffer;
	ULONG Length;
	UNICODE_STRING KeyName, Expected;
	ULONG Major, Minor;
	PCWSTR Version, Build;

	if (!Name || !NT_SUCCESS(KexDataInitialize(&Data)) || !Data ||
		!(Data->IfeoParameters.StrongVersionSpoof & KEX_STRONGSPOOF_REGISTRY) ||
		Data->IfeoParameters.WinVerSpoof == WinVerSpoofNone ||
		AshModuleIsWindowsModule(Caller)) {
		return FALSE;
	}
	if (!NT_SUCCESS(NtQueryKey(Key, KeyNameInformation, Buffer, sizeof(Buffer), &Length))) {
		return FALSE;
	}
	KeyName.Buffer = Info->Name;
	KeyName.Length = (USHORT) Info->NameLength;
	KeyName.MaximumLength = KeyName.Length;
	RtlInitUnicodeString(&Expected, L"\\REGISTRY\\MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion");
	if (!RtlEqualUnicodeString(&KeyName, &Expected, TRUE)) {
		RtlInitUnicodeString(&Expected, L"\\REGISTRY\\MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows NT\\CurrentVersion");
		if (!RtlEqualUnicodeString(&KeyName, &Expected, TRUE)) return FALSE;
	}

	switch (Data->IfeoParameters.WinVerSpoof) {
	case WinVerSpoofWin7: Major = 6; Minor = 1; Version = L"6.1"; Build = L"7601"; break;
	case WinVerSpoofWin8: Major = 6; Minor = 2; Version = L"6.2"; Build = L"9200"; break;
	case WinVerSpoofWin8Point1: Major = 6; Minor = 3; Version = L"6.3"; Build = L"9600"; break;
	case WinVerSpoofWin10: Major = 10; Minor = 0; Version = L"10.0"; Build = L"19045"; break;
	case WinVerSpoofWin11: Major = 10; Minor = 0; Version = L"10.0"; Build = L"26200"; break;
	default: return FALSE;
	}
	*Type = REG_SZ;
	if (StringEqualIW(Name, L"CurrentVersion")) *Text = Version;
	else if (StringEqualIW(Name, L"CurrentBuild") || StringEqualIW(Name, L"CurrentBuildNumber")) *Text = Build;
	else if (StringEqualIW(Name, L"CurrentMajorVersionNumber")) { *Type = REG_DWORD; *Number = Major; }
	else if (StringEqualIW(Name, L"CurrentMinorVersionNumber")) { *Type = REG_DWORD; *Number = Minor; }
	else return FALSE;
	return TRUE;
}

STATIC LSTATUS KxCopyVersionValue(ULONG Type, ULONG Number, PCWSTR Text,
	BOOLEAN Wide, LPDWORD OutputType, LPBYTE Output, LPDWORD Size)
{
	ULONG Required, Capacity = (Output && Size) ? *Size : 0;
	ULONG Index;
	if (Output && !Size) return ERROR_INVALID_PARAMETER;
	Required = Type == REG_DWORD ? sizeof(ULONG) :
		(ULONG) ((wcslen(Text) + 1) * (Wide ? sizeof(WCHAR) : sizeof(CHAR)));
	if (OutputType) *OutputType = Type;
	if (Size) *Size = Required;
	if (!Output) return ERROR_SUCCESS;
	if (Capacity < Required) return ERROR_MORE_DATA;
	if (Type == REG_DWORD) RtlCopyMemory(Output, &Number, sizeof(Number));
	else if (Wide) RtlCopyMemory(Output, Text, Required);
	else for (Index = 0; Index < Required; ++Index) Output[Index] = (BYTE) Text[Index];
	return ERROR_SUCCESS;
}

LSTATUS WINAPI Ext_RegQueryValueExW(HKEY Key, LPCWSTR Name, LPDWORD Reserved,
	LPDWORD Type, LPBYTE Value, LPDWORD Size)
{
	ULONG FakeType, Number = 0, ActualSize = 0;
	PCWSTR Text = NULL;
	LSTATUS Status;
	if (!Reserved && KxVersionRegistryValue(Key, Name, ReturnAddress(), &FakeType, &Number, &Text)) {
		// Preserve access checks even for synthetic Win10 DWORD values on Vista.
		Status = RegQueryValueExW(Key, Name, NULL, NULL, NULL, &ActualSize);
		if (Status != ERROR_SUCCESS && Status != ERROR_FILE_NOT_FOUND) return Status;
		return KxCopyVersionValue(FakeType, Number, Text, TRUE, Type, Value, Size);
	}
	return RegQueryValueExW(Key, Name, Reserved, Type, Value, Size);
}

LSTATUS WINAPI Ext_RegQueryValueExA(HKEY Key, LPCSTR Name, LPDWORD Reserved,
	LPDWORD Type, LPBYTE Value, LPDWORD Size)
{
	WCHAR WideName[64];
	ULONG FakeType, Number = 0, ActualSize = 0;
	PCWSTR Text = NULL;
	LSTATUS Status;
	if (!Reserved && Name && MultiByteToWideChar(CP_ACP, 0, Name, -1, WideName, ARRAYSIZE(WideName)) &&
		KxVersionRegistryValue(Key, WideName, ReturnAddress(), &FakeType, &Number, &Text)) {
		Status = RegQueryValueExA(Key, Name, NULL, NULL, NULL, &ActualSize);
		if (Status != ERROR_SUCCESS && Status != ERROR_FILE_NOT_FOUND) return Status;
		return KxCopyVersionValue(FakeType, Number, Text, FALSE, Type, Value, Size);
	}
	return RegQueryValueExA(Key, Name, Reserved, Type, Value, Size);
}
