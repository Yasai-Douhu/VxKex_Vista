///////////////////////////////////////////////////////////////////////////////
//
// Module Name:
//
//     stubs.c
//
// Abstract:
//
//     Forwarder stubs that do nothing except for calling the original function.
//     These exist because some stupid software such as THEMIDA is not
//     compatible with export forwarders.
//
// Author:
//
//     vxiiduu (27-Apr-2026)
//
// Environment:
//
//     Native
//
// Revision History:
//
//     vxiiduu              27-Apr-2026  Initial creation.
//
///////////////////////////////////////////////////////////////////////////////

#include "buildcfg.h"
#include <KexComm.h>
#include <KexDll.h>

NTSTATUS NTAPI Stub_Ext_RtlInitializeCriticalSection(
	IN	PRTL_CRITICAL_SECTION	CriticalSection)
{
	return Ext_RtlInitializeCriticalSection(
		CriticalSection);
}

NTSTATUS NTAPI Stub_Ext_NtSetInformationThread(
	IN	HANDLE				ThreadHandle,
	IN	THREADINFOCLASS		ThreadInformationClass,
	IN	PVOID				ThreadInformation,
	IN	ULONG				ThreadInformationLength)
{
	return Ext_NtSetInformationThread(
		ThreadHandle,
		ThreadInformationClass,
		ThreadInformation,
		ThreadInformationLength);
}

NTSTATUS NTAPI Stub_NtQueryVirtualMemory(
	IN		HANDLE			ProcessHandle,
	IN		PVOID			BaseAddress OPTIONAL,
	IN		MEMINFOCLASS	MemoryInformationClass,
	OUT		PVOID			MemoryInformation,
	IN		SIZE_T			MemoryInformationLength,
	OUT		PSIZE_T			ReturnLength OPTIONAL)
{
	return KexNtQueryVirtualMemory(
		ProcessHandle,
		BaseAddress,
		MemoryInformationClass,
		MemoryInformation,
		MemoryInformationLength,
		ReturnLength);
}

NTSTATUS NTAPI Stub_Ext_NtQueryInformationProcess(
	IN	HANDLE				ProcessHandle,
	IN	PROCESSINFOCLASS	ProcessInformationClass,
	OUT	PVOID				ProcessInformation,
	IN	ULONG				ProcessInformationLength,
	OUT	PULONG				ReturnLength OPTIONAL)
{
	return Ext_NtQueryInformationProcess(
		ProcessHandle,
		ProcessInformationClass,
		ProcessInformation,
		ProcessInformationLength,
		ReturnLength);
}

// Vista lacks NtFlushBuffersFileEx.  The older call performs a full flush,
// which also provides the durability requested by data-only flushes.
NTSTATUS NTAPI KexNtFlushBuffersFileEx(
	IN	HANDLE				FileHandle,
	IN	ULONG				Flags,
	IN	PVOID				Parameters,
	IN	ULONG				ParametersSize,
	OUT	PIO_STATUS_BLOCK	IoStatusBlock)
{
	if (!IoStatusBlock || Parameters || ParametersSize ||
		(Flags != 0 && Flags != 1 && Flags != 2 && Flags != 4)) {
		return STATUS_INVALID_PARAMETER;
	}

	return NtFlushBuffersFile(FileHandle, IoStatusBlock);
}
