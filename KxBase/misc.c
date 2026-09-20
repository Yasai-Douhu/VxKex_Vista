#include "buildcfg.h"
#include "kxbasep.h"

KXBASEAPI BOOL WINAPI Ext_GetThreadGroupAffinity(HANDLE Thread, PGROUP_AFFINITY Affinity)
{
	THREAD_BASIC_INFORMATION Information;
	NTSTATUS Status;
	if (!Affinity) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	Status = NtQueryInformationThread(Thread, ThreadBasicInformation,
		&Information, sizeof(Information), NULL);
	if (!NT_SUCCESS(Status)) {
		SetLastError(RtlNtStatusToDosError(Status));
		return FALSE;
	}
	RtlZeroMemory(Affinity, sizeof(*Affinity));
	Affinity->Mask = Information.AffinityMask;
	return TRUE;
}

// Vista has no kernel power-request objects. Report lack of support instead
// of returning a fake handle or claiming that sleep prevention succeeded.
KXBASEAPI HANDLE WINAPI Ext_PowerCreateRequest(PVOID Reason)
{
	SetLastError(ERROR_NOT_SUPPORTED);
	return INVALID_HANDLE_VALUE;
}
KXBASEAPI BOOL WINAPI Ext_PowerSetRequest(HANDLE Request, ULONG Type)
{
	SetLastError(ERROR_NOT_SUPPORTED);
	return FALSE;
}
KXBASEAPI BOOL WINAPI Ext_PowerClearRequest(HANDLE Request, ULONG Type)
{
	SetLastError(ERROR_NOT_SUPPORTED);
	return FALSE;
}

KXBASEAPI VOID WINAPI Ext_RaiseFailFastException(
	PEXCEPTION_RECORD Record, PCONTEXT Context, DWORD Flags)
{
	typedef NTSTATUS (NTAPI *PRAISE)(PEXCEPTION_RECORD, PCONTEXT, BOOLEAN);
	EXCEPTION_RECORD LocalRecord;
	CONTEXT LocalContext;
	PRAISE Raise;
	if (!Record) {
		RtlZeroMemory(&LocalRecord, sizeof(LocalRecord));
		LocalRecord.ExceptionCode = (DWORD) 0xC0000602;
		LocalRecord.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
		LocalRecord.ExceptionAddress = _ReturnAddress();
		Record = &LocalRecord;
	}
	if (!Context) {
		RtlCaptureContext(&LocalContext);
		Context = &LocalContext;
	}
	Raise = (PRAISE) GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtRaiseException");
	if (Raise) Raise(Record, Context, FALSE); // Second chance: bypass handlers.
	TerminateProcess(GetCurrentProcess(), Record->ExceptionCode);
}

KXBASEAPI int WINAPI Ext_ResolveLocaleName(LPCWSTR Name, LPWSTR Output, int Count)
{
	LCID Locale;
	WCHAR Candidate[LOCALE_NAME_MAX_LENGTH];
	PWCHAR Separator;
	if (Count < 0 || (!Output && Count != 0)) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return 0;
	}
	Locale = LocaleNameToLCID(Name, 0);
	if (!Locale && Name && wcslen(Name) < ARRAYSIZE(Candidate)) {
		wcscpy(Candidate, Name);
		while (!Locale && (Separator = wcsrchr(Candidate, L'-')) != NULL) {
			*Separator = 0;
			Locale = LocaleNameToLCID(Candidate, 0);
		}
	}
	if (!Locale) return 0;
	return LCIDToLocaleName(Locale, Output, Count, 0);
}

KXBASEAPI BOOL WINAPI GetOsSafeBootMode(
	OUT	PBOOL	IsSafeBootMode)
{
	*IsSafeBootMode = FALSE;
	return TRUE;
}

KXBASEAPI BOOL WINAPI GetFirmwareType(
	OUT	PFIRMWARE_TYPE	FirmwareType)
{
	*FirmwareType = FirmwareTypeUnknown;
	return TRUE;
}
KXBASEAPI DWORD WINAPI Ext_GetActiveProcessorCount(IN WORD GroupNumber) { SYSTEM_INFO si; GetSystemInfo(&si); return si.dwNumberOfProcessors; }
KXBASEAPI WORD WINAPI Ext_GetActiveProcessorGroupCount(VOID) { return 1; }
KXBASEAPI DWORD WINAPI Ext_GetMaximumProcessorCount(IN WORD GroupNumber) { SYSTEM_INFO si; GetSystemInfo(&si); return si.dwNumberOfProcessors; }
KXBASEAPI WORD WINAPI Ext_GetMaximumProcessorGroupCount(VOID) { return 1; }

// NT 6.0 rejects the Win7 directory-enumeration optimizations used by Chromium.
// Preserve all other flags/search criteria, including errors for unknown flags.
KXBASEAPI HANDLE WINAPI Ext_FindFirstFileExW(
    LPCWSTR Path, FINDEX_INFO_LEVELS Level, LPVOID Data,
    FINDEX_SEARCH_OPS Search, LPVOID Filter, DWORD Flags)
{
    if (OriginalMajorVersion == 6 && OriginalMinorVersion == 0) {
        if (Level == FindExInfoBasic) Level = FindExInfoStandard;
        Flags &= ~FIND_FIRST_EX_LARGE_FETCH;
    }
    return FindFirstFileExW(Path, Level, Data, Search, Filter, Flags);
}

KXBASEAPI HANDLE WINAPI Ext_FindFirstFileExA(
    LPCSTR Path, FINDEX_INFO_LEVELS Level, LPVOID Data,
    FINDEX_SEARCH_OPS Search, LPVOID Filter, DWORD Flags)
{
    if (OriginalMajorVersion == 6 && OriginalMinorVersion == 0) {
        if (Level == FindExInfoBasic) Level = FindExInfoStandard;
        Flags &= ~FIND_FIRST_EX_LARGE_FETCH;
    }
    return FindFirstFileExA(Path, Level, Data, Search, Filter, Flags);
}
