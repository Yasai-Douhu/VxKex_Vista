#include "buildcfg.h"
#include "kxbasep.h"

STATIC ULONGLONG (NTAPI *KxBasepQueryTickCount64)(VOID);

STATIC ULONGLONG WINAPI KxBasepGetTickCount64Hook(VOID)
{
	return KxBasepQueryTickCount64();
}

STATIC DWORD WINAPI KxBasepGetTickCountHook(VOID)
{
	return (DWORD) KxBasepQueryTickCount64();
}

VOID KxBasepInitializeTickCountHooks(VOID)
{
	// Resolve dynamically so this DLL can still load with an older KexDll.
	KxBasepQueryTickCount64 = (ULONGLONG (NTAPI *)(VOID))
		GetProcAddress((HMODULE) KexData->KexDllBase, "KexQueryTickCount64");
	if (KxBasepQueryTickCount64) {
		KexHkInstallBasicHook(GetTickCount, KxBasepGetTickCountHook, NULL);
		KexHkInstallBasicHook(GetTickCount64, KxBasepGetTickCount64Hook, NULL);
	}
}

// Windows 7 added wake reasons and timer coalescing. Vista can still deliver
// the timer/APC through SetWaitableTimer; not coalescing stays within the
// caller's permitted delay. Preserve the full API on systems which have it.
KXBASEAPI BOOL WINAPI Ext_SetWaitableTimerEx(
	IN HANDLE Timer,
	IN CONST LARGE_INTEGER *DueTime,
	IN LONG Period,
	IN PTIMERAPCROUTINE CompletionRoutine OPTIONAL,
	IN PVOID CompletionArgument OPTIONAL,
	IN PVOID WakeContext OPTIONAL,
	IN ULONG TolerableDelay)
{
	typedef BOOL (WINAPI *PSET_TIMER_EX)(HANDLE, CONST LARGE_INTEGER *, LONG,
		PTIMERAPCROUTINE, PVOID, PVOID, ULONG);
	PSET_TIMER_EX Native;
	Native = (PSET_TIMER_EX) GetProcAddress(GetModuleHandleW(L"kernel32.dll"),
		"SetWaitableTimerEx");
	if (Native) {
		return Native(Timer, DueTime, Period, CompletionRoutine,
			CompletionArgument, WakeContext, TolerableDelay);
	}
	return SetWaitableTimer(Timer, DueTime, Period, CompletionRoutine,
		CompletionArgument, WakeContext != NULL);
}

//
// If strong SharedUserData spoofing is enabled, this function
// supersedes KernelBase!GetSystemTimeAsFileTime because the original
// function reads system time from SharedUserData.
//
KXBASEAPI VOID WINAPI KxBasepGetSystemTimeAsFileTimeHook(
	OUT	PFILETIME	SystemTimeAsFileTime)
{
	ASSERT (KexData->IfeoParameters.StrongVersionSpoof & KEX_STRONGSPOOF_SHAREDUSERDATA);
	NtQuerySystemTime((PLONGLONG) SystemTimeAsFileTime);
}

//
// Same as above but this function supersedes GetSystemTime when doing
// SharedUserData-based version spoofing.
//
KXBASEAPI VOID WINAPI KxBasepGetSystemTimeHook(
	OUT	PSYSTEMTIME	SystemTime)
{
	LONGLONG Time;
	TIME_FIELDS TimeFields;

	ASSERT (KexData->IfeoParameters.StrongVersionSpoof & KEX_STRONGSPOOF_SHAREDUSERDATA);

	NtQuerySystemTime(&Time);
	RtlTimeToTimeFields(&Time, &TimeFields);

	//
	// Annoyingly, the TIME_FIELDS structure is not directly compatible with
	// the SYSTEMTIME structure...
	//

	SystemTime->wYear			= TimeFields.Year;
	SystemTime->wMonth			= TimeFields.Month;
	SystemTime->wDay			= TimeFields.Day;
	SystemTime->wDayOfWeek		= TimeFields.Weekday;
	SystemTime->wHour			= TimeFields.Hour;
	SystemTime->wMinute			= TimeFields.Minute;
	SystemTime->wSecond			= TimeFields.Second;
	SystemTime->wMilliseconds	= TimeFields.Milliseconds;
}

KXBASEAPI VOID WINAPI GetSystemTimePreciseAsFileTime(
	OUT	PFILETIME	SystemTimeAsFileTime)
{
	//
	// The real NtQuerySystemTime export from NTDLL is actually just a jump to
	// RtlQuerySystemTime, which reads from SharedUserData.
	//
	// However, if we are doing SharedUserData-based version spoofing, we will
	// overwrite that stub function with KexNtQuerySystemTime, so it is the best
	// of both worlds in terms of speed and actually working.
	//

	NtQuerySystemTime((PLONGLONG) SystemTimeAsFileTime);
}

KXBASEAPI VOID WINAPI QueryUnbiasedInterruptTimePrecise(
	OUT	PULONGLONG	UnbiasedInterruptTimePrecise)
{
	Ext_QueryUnbiasedInterruptTime(UnbiasedInterruptTimePrecise);
}
KXBASEAPI BOOL WINAPI Ext_QueryUnbiasedInterruptTime(PULONGLONG UnbiasedTime) { if (UnbiasedTime) { *UnbiasedTime = GetTickCount64() * 10000; return TRUE; } return FALSE; }
