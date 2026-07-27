///////////////////////////////////////////////////////////////////////////////
//
// Module Name:
//
//     synch.c
//
// Abstract:
//
//     スレッド同期に関する関数群を含む。
//     TryAcquireSRWLock 系は Vista/ntdll に存在しないため直接実装する。
//
// Author:
//
//     vxiiduu (11-Feb-2022)
//
// Environment:
//
//     Win32 mode.
//
// Revision History:
//
//     vxiiduu              11-Feb-2022  Initial creation.
//     (VxKex Vista port)  2026         TryAcquireSRWLock 実装を追加。
//
///////////////////////////////////////////////////////////////////////////////

#include "buildcfg.h"
#include "kxbasep.h"

//
// SRW ロックの内部ビットフィールド定義。
// Windows の RTL_SRWLOCK は PVOID (ポインタサイズ) の値を持ち、
// 以下のビットが使われる:
//   bit 0  = ロック状態フラグ (Locked)
//   bit 1  = 共有ロック待機者あり
//   その他 = 共有ロックカウント等
//
// TryAcquire の実装はアトミック比較交換で未ロック状態からロック状態へ遷移する。
//

// 排他ロック取得を試みる (ブロックしない)
// Vista の kernel32/ntdll には存在しないため KxBase で実装する。
KXBASEAPI BOOLEAN WINAPI TryAcquireSRWLockExclusive(
	IN OUT PRTL_SRWLOCK SRWLock)
{
	// SRW ロックが 0 (未ロック) なら 1 (排他ロック) にアトミックに置き換える
	return (InterlockedCompareExchangePointer(
		(volatile PVOID*)&SRWLock->Ptr,
		(PVOID)1,
		(PVOID)0) == (PVOID)0);
}

// 共有ロック取得を試みる (ブロックしない)
// Vista の kernel32/ntdll には存在しないため KxBase で実装する。
KXBASEAPI BOOLEAN WINAPI TryAcquireSRWLockShared(
	IN OUT PRTL_SRWLOCK SRWLock)
{
	LONG_PTR OldValue;
	LONG_PTR NewValue;

	do {
		OldValue = (LONG_PTR)(volatile PVOID)SRWLock->Ptr;

		// bit 0 が立っていれば排他ロック中なので失敗
		if (OldValue & 1) {
			return FALSE;
		}

		// 共有カウントを +4 (共有カウントは bit 2 以上に格納)
		NewValue = OldValue + 4;
	} while (InterlockedCompareExchangePointer(
		(volatile PVOID*)&SRWLock->Ptr,
		(PVOID)NewValue,
		(PVOID)OldValue) != (PVOID)OldValue);

	return TRUE;
}

//
// This function is a wrapper around (Kex)RtlWaitOnAddress.
//
KXBASEAPI BOOL WINAPI WaitOnAddress(
	IN	VOLATILE VOID	*Address,
	IN	PVOID			CompareAddress,
	IN	SIZE_T			AddressSize,
	IN	DWORD			Milliseconds OPTIONAL)
{
	NTSTATUS Status;
	PLARGE_INTEGER TimeOutPointer;
	LARGE_INTEGER TimeOut;

	TimeOutPointer = BaseFormatTimeOut(&TimeOut, Milliseconds);

	Status = KexRtlWaitOnAddress(
		Address,
		CompareAddress,
		AddressSize,
		TimeOutPointer);

	BaseSetLastNTError(Status);
	
	if (NT_SUCCESS(Status) && Status != STATUS_TIMEOUT) {
		return TRUE;
	} else {
		return FALSE;
	}
}