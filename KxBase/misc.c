#include "buildcfg.h"
#include "kxbasep.h"

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
