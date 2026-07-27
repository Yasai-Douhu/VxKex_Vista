///////////////////////////////////////////////////////////////////////////////
//
// Module Name:
//
//     kxuserp.h
//
// Abstract:
//
//     Private header file for KxUser.
//
// Author:
//
//     vxiiduu (10-Feb-2022)
//
// Revision History:
//
//     vxiiduu              10-Feb-2022  Initial creation.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <KexComm.h>
#include <KexDll.h>
#include <KxUser.h>

EXTERN PKEX_PROCESS_DATA KexData;

typedef struct _FNDWORDMSG {
	PVOID		pwnd;
	UINT		msg;
	WPARAM		wParam;
	LPARAM		lParam;
	ULONG_PTR	xParam;
	PVOID		xpfnProc;
} TYPEDEF_TYPE_NAME(FNDWORDMSG);

typedef struct _TOUCH_POINT_ENTRY {
	DWORD				PointerId;
	BOOL				Valid;
	BOOL				Active;
	POINTER_TOUCH_INFO	TouchInfo;
} TYPEDEF_TYPE_NAME(TOUCH_POINT_ENTRY);

typedef struct _PEN_POINT_ENTRY {
	DWORD				PointerId;
	BOOL				Valid;
	BOOL				Active;
	POINTER_PEN_INFO	PenInfo;
} TYPEDEF_TYPE_NAME(PEN_POINT_ENTRY);

//
// loadsyslib.c
//

HMODULE LoadSystemLibrary(
	LPCWSTR	FileName);

//
// pointer.c
//

KXUSERAPI BOOL WINAPI IsMouseInPointerEnabled(
	VOID);

//
// winmsg.c
//

VOID InitializeTouchAndPenPoints(
	VOID);

VOID CleanupTouchAndPenPoints(
	VOID);

PTOUCH_POINT_ENTRY FindOrCreateTouchPoint(
	DWORD	PointerId);

VOID DeactivateTouchPoint(
	DWORD	PointerId);

VOID RemoveTouchPoint(
	DWORD	PointerId);

PPEN_POINT_ENTRY FindOrCreatePenPoint(
	DWORD	PointerId);

VOID DeactivatePenPoint(
	DWORD	PointerId);

VOID RemovePenPoint(
	DWORD	PointerId);

PVOID FindOrCreateTouchOrPenPoint(
	DWORD	PointerId,
	BOOL	IsPenMessage);

VOID DeactivateTouchOrPenPoint(
	DWORD	PointerId,
	BOOL	IsPenMessage);

VOID RemoveTouchOrPenPoint(
	DWORD	PointerId,
	BOOL	IsPenMessage);

NTSTATUS EnableWindowMessageInterception(
	VOID);

//
// syscal.c
//

#if defined(KEX_ARCH_X64)

#define KXUSER_DECLARE_SYSCALL(SyscallName, ...) \
KXUSERAPI NTSTATUS NTAPI KxUser##SyscallName##_Win7(__VA_ARGS__); \
KXUSERAPI NTSTATUS NTAPI KxUser##SyscallName(__VA_ARGS__);

#else

#define KXUSER_DECLARE_SYSCALL(SyscallName, ...) \
KXUSERAPI NTSTATUS NTAPI KxUser##SyscallName##_Win7_Native32(__VA_ARGS__); \
KXUSERAPI NTSTATUS NTAPI KxUser##SyscallName##_Win7_Wow64(__VA_ARGS__); \
KXUSERAPI NTSTATUS NTAPI KxUser##SyscallName(__VA_ARGS__);

#endif

#ifndef HTOUCHINPUT
DECLARE_HANDLE(HTOUCHINPUT);
typedef struct tagTOUCHINPUT {
    LONG x;
    LONG y;
    HANDLE hSource;
    DWORD dwID;
    DWORD dwFlags;
    DWORD dwMask;
    DWORD dwTime;
    ULONG_PTR dwExtraInfo;
    DWORD cxContact;
    DWORD cyContact;
} TOUCHINPUT, *PTOUCHINPUT;
#endif

#ifndef WM_TOUCH
#define WM_TOUCH 0x0240
#endif

#ifndef TOUCHEVENTF_MOVE
#define TOUCHEVENTF_MOVE 0x0001
#define TOUCHEVENTF_DOWN 0x0002
#define TOUCHEVENTF_UP 0x0004
#define TOUCHEVENTF_INRANGE 0x0008
#define TOUCHEVENTF_PRIMARY 0x0010
#define TOUCHEVENTF_NOCOALESCE 0x0020
#define TOUCHEVENTF_PEN 0x0040
#define TOUCHEVENTF_PALM 0x0080
#endif

KXUSER_DECLARE_SYSCALL(NtUserGetTouchInputInfo,
	HTOUCHINPUT	TouchInput,
	UINT		Inputs,
	PTOUCHINPUT	InputsPtr,
	int			Size);