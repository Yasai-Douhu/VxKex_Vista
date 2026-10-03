#include "buildcfg.h"
#include "kexdllp.h"

// An auto-reset event provides the single pending notification and an atomic
// timeout/wake race. Thread handles pin identities, not TEB addresses. Retire
// signalled threads before opening IDs so our cache cannot keep dead IDs alive.
typedef struct _KEX_ALERT_STATE {
	struct _KEX_ALERT_STATE *Next;
	HANDLE ThreadId;
	HANDLE Thread;
	HANDLE Event;
} KEX_ALERT_STATE;

static SRWLOCK AlertLock;
static KEX_ALERT_STATE *AlertStates;
static PVOID volatile NativeAlert;
static PVOID volatile NativeWait;

static PVOID KexResolveNativeAlert(PVOID volatile *Cache, PCSTR Name)
{
	PVOID Address = InterlockedCompareExchangePointer(Cache, NULL, NULL);
	if (!Address) {
		PVOID Found = GetProcAddress(GetModuleHandleW(L"ntdll.dll"), Name);
		if (!Found) Found = (PVOID) 1;
		Address = InterlockedCompareExchangePointer(Cache, Found, NULL);
		if (!Address) Address = Found;
	}
	return Address == (PVOID) 1 ? NULL : Address;
}

static VOID KexFreeAlertState(KEX_ALERT_STATE *State)
{
	NtClose(State->Event);
	NtClose(State->Thread);
	HeapFree(GetProcessHeap(), 0, State);
}

static VOID KexRetireExitedAlertStates(VOID)
{
	KEX_ALERT_STATE **Link = &AlertStates;
	LARGE_INTEGER Zero;
	Zero.QuadPart = 0;
	while (*Link) {
		KEX_ALERT_STATE *State = *Link;
		if (NtWaitForSingleObject(State->Thread, FALSE, &Zero) == STATUS_SUCCESS) {
			*Link = State->Next;
			KexFreeAlertState(State);
		} else {
			Link = &State->Next;
		}
	}
}

// Explicit unload requires the caller to have stopped all calls into the DLL.
// During process termination the kernel reclaims handles; avoid taking locks.
VOID KexCleanupAlertByThreadId(VOID)
{
	RtlAcquireSRWLockExclusive(&AlertLock);
	while (AlertStates) {
		KEX_ALERT_STATE *State = AlertStates;
		AlertStates = State->Next;
		KexFreeAlertState(State);
	}
	RtlReleaseSRWLockExclusive(&AlertLock);
}

static KEX_ALERT_STATE *KexFindAlertState(HANDLE ThreadId)
{
	KEX_ALERT_STATE *State;
	for (State = AlertStates; State; State = State->Next) {
		if (State->ThreadId == ThreadId) return State;
	}
	return NULL;
}

static NTSTATUS KexCreateAlertState(HANDLE ThreadId, HANDLE Thread,
	KEX_ALERT_STATE **Result)
{
	NTSTATUS Status;
	KEX_ALERT_STATE *State = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*State));
	if (!State) return STATUS_NO_MEMORY;
	Status = NtCreateEvent(&State->Event, EVENT_MODIFY_STATE | SYNCHRONIZE,
		NULL, SynchronizationEvent, FALSE);
	if (!NT_SUCCESS(Status)) {
		HeapFree(GetProcessHeap(), 0, State);
		return Status;
	}
	State->ThreadId = ThreadId;
	State->Thread = Thread;
	State->Next = AlertStates;
	AlertStates = State;
	*Result = State;
	return STATUS_SUCCESS;
}

KEXAPI NTSTATUS NTAPI NtAlertThreadByThreadId(HANDLE UniqueThread)
{
	NTSTATUS Status;
	ULONG SavedError = GetLastError();
	NTSTATUS (NTAPI *Native)(HANDLE) = KexResolveNativeAlert(&NativeAlert, "NtAlertThreadByThreadId");
	HANDLE Thread = NULL;
	OBJECT_ATTRIBUTES Attributes;
	CLIENT_ID ClientId;
	THREAD_BASIC_INFORMATION Basic;
	KEX_ALERT_STATE *State;
	LARGE_INTEGER Zero;
	if (Native) {
		Status = Native(UniqueThread);
		SetLastError(SavedError);
		return Status;
	}
	UniqueThread = (HANDLE) ((ULONG_PTR) UniqueThread & ~((ULONG_PTR) 3));
	InitializeObjectAttributes(&Attributes, NULL, 0, NULL, NULL);
	ClientId.UniqueProcess = NULL;
	ClientId.UniqueThread = UniqueThread;
	Zero.QuadPart = 0;
	RtlAcquireSRWLockExclusive(&AlertLock);
	KexRetireExitedAlertStates();
	State = KexFindAlertState(UniqueThread);
	if (State) {
		// The live, pinned identity was already verified in this process.
		Status = NtSetEvent(State->Event, NULL);
		goto Exit;
	}
	Status = NtOpenThread(&Thread, THREAD_QUERY_LIMITED_INFORMATION | SYNCHRONIZE,
		&Attributes, &ClientId);
	if (!NT_SUCCESS(Status)) {
		if (Status == STATUS_INVALID_PARAMETER) Status = STATUS_INVALID_CID;
		goto Exit;
	}
	Status = NtQueryInformationThread(Thread, ThreadBasicInformation, &Basic, sizeof(Basic), NULL);
	if (!NT_SUCCESS(Status)) goto Exit;
	if (Basic.ClientId.UniqueProcess != NtCurrentTeb()->ClientId.UniqueProcess) {
		Status = STATUS_ACCESS_DENIED;
		goto Exit;
	}
	// Native accepts an exited thread while external references keep its ID
	// alive. Return success without retaining another dead thread reference.
	if (NtWaitForSingleObject(Thread, FALSE, &Zero) == STATUS_SUCCESS) {
		Status = STATUS_SUCCESS;
		goto Exit;
	}
	State = KexFindAlertState(Basic.ClientId.UniqueThread);
	if (!State) {
		Status = KexCreateAlertState(Basic.ClientId.UniqueThread, Thread, &State);
		if (!NT_SUCCESS(Status)) goto Exit;
		Thread = NULL; // ownership transferred to the state
	}
	Status = NtSetEvent(State->Event, NULL);
Exit:
	if (Thread) NtClose(Thread);
	RtlReleaseSRWLockExclusive(&AlertLock);
	SetLastError(SavedError);
	return Status;
}

KEXAPI NTSTATUS NTAPI NtWaitForAlertByThreadId(PVOID Hint, PLARGE_INTEGER Timeout)
{
	NTSTATUS Status;
	ULONG SavedError = GetLastError();
	NTSTATUS (NTAPI *Native)(PVOID, PLARGE_INTEGER) = KexResolveNativeAlert(&NativeWait, "NtWaitForAlertByThreadId");
	LARGE_INTEGER Captured;
	KEX_ALERT_STATE *State;
	HANDLE Thread = NULL, ThreadId = NtCurrentTeb()->ClientId.UniqueThread;
	if (Native) {
		Status = Native(Hint, Timeout);
		SetLastError(SavedError);
		return Status;
	}
	if (Timeout) {
		try { Captured = *Timeout; } except (EXCEPTION_EXECUTE_HANDLER) {
			Status = GetExceptionCode();
			SetLastError(SavedError);
			return Status;
		}
		Timeout = &Captured;
	}
	RtlAcquireSRWLockExclusive(&AlertLock);
	KexRetireExitedAlertStates();
	State = KexFindAlertState(ThreadId);
	if (!State) {
		if (Timeout && !Timeout->QuadPart) {
			Status = STATUS_TIMEOUT;
			goto Failure;
		}
		Status = NtDuplicateObject(NtCurrentProcess(), NtCurrentThread(), NtCurrentProcess(),
			&Thread, 0, 0, DUPLICATE_SAME_ACCESS);
		if (!NT_SUCCESS(Status)) goto Failure;
		Status = KexCreateAlertState(ThreadId, Thread, &State);
		if (!NT_SUCCESS(Status)) goto Failure;
		Thread = NULL;
	}
	RtlReleaseSRWLockExclusive(&AlertLock);
	// The current thread cannot be retired while alive, including while in
	// this wait. No registry lock is held across a blocking operation.
	Status = NtWaitForSingleObject(State->Event, FALSE, Timeout);
	if (Status == STATUS_SUCCESS) Status = STATUS_ALERTED;
	SetLastError(SavedError);
	return Status;
Failure:
	if (Thread) NtClose(Thread);
	RtlReleaseSRWLockExclusive(&AlertLock);
	SetLastError(SavedError);
	return Status;
}
