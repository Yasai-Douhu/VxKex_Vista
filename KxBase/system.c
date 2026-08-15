///////////////////////////////////////////////////////////////////////////////
//
// Module Name:
//
//     system.c
//
// Abstract:
//
//     Windows 7 以降でのみ利用可能な API の互換実装を提供する。
//     - GetLogicalProcessorInformationEx
//     - GetProcessGroupAffinity
//     - K32GetProcessMemoryInfo
//     これらは Vista/Server 2008 の kernel32.dll に存在しないため、
//     KxBase 内で自前実装する必要がある。
//
// Author:
//
//     VxKex Vista Port (2026)
//
// Environment:
//
//     Win32 mode.
//
///////////////////////////////////////////////////////////////////////////////

#include "buildcfg.h"
#include "kxbasep.h"
#include <Psapi.h>

//
// SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX 関連の構造体定義。
// _WIN32_WINNT >= 0x0601 であれば SDK ヘッダーに含まれるが、
// NUMA_NODE_RELATIONSHIP の GroupCount/GroupMasks フィールドは
// Windows Server 2022 (build 20348) 以降でしか存在しないため、
// GroupMask (単数) のみを使用する。
//

//
// GetLogicalProcessorInformationEx の互換実装。
//
// Vista には GetLogicalProcessorInformation (旧形式) が存在するため、
// それを呼び出して SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX 形式に
// 変換する。Server 2008 はプロセッサグループ非対応のため、
// 常にグループ 0 として報告する。
//
KXBASEAPI BOOL WINAPI GetLogicalProcessorInformationEx(
	IN	LOGICAL_PROCESSOR_RELATIONSHIP	RelationshipType,
	OUT	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX Buffer OPTIONAL,
	IN OUT PDWORD ReturnedLength)
{
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION OldBuffer = NULL;
	DWORD OldBufferSize = 0;
	DWORD RequiredSize;
	BOOL Success;
	DWORD i, Count;
	PBYTE WritePtr;

	if (!ReturnedLength) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	//
	// まず旧 API でバッファサイズを取得する
	//
	GetLogicalProcessorInformation(NULL, &OldBufferSize);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
		return FALSE;
	}

	OldBuffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)HeapAlloc(
		GetProcessHeap(), 0, OldBufferSize);
	if (!OldBuffer) {
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return FALSE;
	}

	Success = GetLogicalProcessorInformation(OldBuffer, &OldBufferSize);
	if (!Success) {
		HeapFree(GetProcessHeap(), 0, OldBuffer);
		return FALSE;
	}

	Count = OldBufferSize / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);

	//
	// 必要なバッファサイズを計算する。
	// 各エントリは SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX に変換される。
	// RelationshipType に合致するエントリのみを返す。
	//
	RequiredSize = 0;
	for (i = 0; i < Count; i++) {
		if (RelationshipType != RelationAll &&
			OldBuffer[i].Relationship != RelationshipType) {
			continue;
		}

		switch (OldBuffer[i].Relationship) {
		case RelationProcessorCore:
		case RelationProcessorPackage:
			RequiredSize += FIELD_OFFSET(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, Processor)
				+ sizeof(PROCESSOR_RELATIONSHIP);
			break;
		case RelationNumaNode:
			RequiredSize += FIELD_OFFSET(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, NumaNode)
				+ sizeof(NUMA_NODE_RELATIONSHIP);
			break;
		case RelationCache:
			RequiredSize += FIELD_OFFSET(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, Cache)
				+ sizeof(CACHE_RELATIONSHIP);
			break;
		default:
			break;
		}
	}

	if (!Buffer || *ReturnedLength < RequiredSize) {
		*ReturnedLength = RequiredSize;
		HeapFree(GetProcessHeap(), 0, OldBuffer);
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return FALSE;
	}

	//
	// バッファに変換結果を書き込む
	//
	WritePtr = (PBYTE)Buffer;

	for (i = 0; i < Count; i++) {
		PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX ExInfo;
		DWORD EntrySize;

		if (RelationshipType != RelationAll &&
			OldBuffer[i].Relationship != RelationshipType) {
			continue;
		}

		ExInfo = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)WritePtr;
		ExInfo->Relationship = OldBuffer[i].Relationship;

		switch (OldBuffer[i].Relationship) {
		case RelationProcessorCore:
		case RelationProcessorPackage:
			EntrySize = FIELD_OFFSET(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, Processor)
				+ sizeof(PROCESSOR_RELATIONSHIP);
			ExInfo->Size = EntrySize;

			// PROCESSOR_RELATIONSHIP を構築する
			ZeroMemory(&ExInfo->Processor, sizeof(PROCESSOR_RELATIONSHIP));
			ExInfo->Processor.Flags = (OldBuffer[i].Relationship == RelationProcessorCore)
				? (BYTE)OldBuffer[i].ProcessorCore.Flags : 0;
			ExInfo->Processor.GroupCount = 1;
			ExInfo->Processor.GroupMask[0].Mask = OldBuffer[i].ProcessorMask;
			ExInfo->Processor.GroupMask[0].Group = 0;
			break;

		case RelationNumaNode:
			EntrySize = FIELD_OFFSET(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, NumaNode)
				+ sizeof(NUMA_NODE_RELATIONSHIP);
			ExInfo->Size = EntrySize;

			ZeroMemory(&ExInfo->NumaNode, sizeof(NUMA_NODE_RELATIONSHIP));
			ExInfo->NumaNode.NodeNumber = OldBuffer[i].NumaNode.NodeNumber;
			// Vista/Win7 SDK の NUMA_NODE_RELATIONSHIP は GroupMask (単数) を持つ
			ExInfo->NumaNode.GroupMask.Mask = OldBuffer[i].ProcessorMask;
			ExInfo->NumaNode.GroupMask.Group = 0;
			break;

		case RelationCache:
			EntrySize = FIELD_OFFSET(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, Cache)
				+ sizeof(CACHE_RELATIONSHIP);
			ExInfo->Size = EntrySize;

			ZeroMemory(&ExInfo->Cache, sizeof(CACHE_RELATIONSHIP));
			ExInfo->Cache.Level = OldBuffer[i].Cache.Level;
			ExInfo->Cache.Associativity = OldBuffer[i].Cache.Associativity;
			ExInfo->Cache.LineSize = (WORD)OldBuffer[i].Cache.LineSize;
			ExInfo->Cache.CacheSize = OldBuffer[i].Cache.Size;
			ExInfo->Cache.Type = OldBuffer[i].Cache.Type;
			ExInfo->Cache.GroupMask.Mask = OldBuffer[i].ProcessorMask;
			ExInfo->Cache.GroupMask.Group = 0;
			break;

		default:
			// 不明な Relationship はスキップする
			continue;
		}

		WritePtr += EntrySize;
	}

	*ReturnedLength = RequiredSize;
	HeapFree(GetProcessHeap(), 0, OldBuffer);
	return TRUE;
}

//
// GetProcessGroupAffinity の互換実装。
//
// Server 2008 / Vista はプロセッサグループをサポートしないため、
// 常にグループ 0 のみを返す。
//
KXBASEAPI BOOL WINAPI GetProcessGroupAffinity(
	IN		HANDLE	hProcess,
	IN OUT	PUSHORT	GroupCount,
	OUT		PUSHORT	GroupArray)
{
	if (!GroupCount) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	if (*GroupCount < 1) {
		*GroupCount = 1;
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return FALSE;
	}

	// Vista ではプロセッサグループは 1 つのみ (グループ 0)
	GroupArray[0] = 0;
	*GroupCount = 1;
	return TRUE;
}

//
// K32GetProcessMemoryInfo の互換実装。
//
// Windows 7 以降、GetProcessMemoryInfo は kernel32.dll から
// K32GetProcessMemoryInfo としてエクスポートされている。
// Vista / Server 2008 では psapi.dll の GetProcessMemoryInfo を
// 動的にロードして委譲する。
//
typedef BOOL (WINAPI *PFN_GETPROCESSMEMORYINFO)(
	HANDLE Process,
	PPROCESS_MEMORY_COUNTERS ppsmemCounters,
	DWORD cb);

KXBASEAPI BOOL WINAPI K32GetProcessMemoryInfo(
	IN	HANDLE					Process,
	OUT	PPROCESS_MEMORY_COUNTERS ppsmemCounters,
	IN	DWORD					cb)
{
	static PFN_GETPROCESSMEMORYINFO pfnGetProcessMemoryInfo = NULL;
	static BOOLEAN Initialized = FALSE;

	if (!Initialized) {
		HMODULE hPsapi = LoadLibraryW(L"psapi.dll");
		if (hPsapi) {
			pfnGetProcessMemoryInfo = (PFN_GETPROCESSMEMORYINFO)
				GetProcAddress(hPsapi, "GetProcessMemoryInfo");
		}
		Initialized = TRUE;
	}

	if (!pfnGetProcessMemoryInfo) {
		SetLastError(ERROR_PROC_NOT_FOUND);
		return FALSE;
	}

	return pfnGetProcessMemoryInfo(Process, ppsmemCounters, cb);
}
