#pragma once
#include "buildcfg.h"
#include <KexComm.h>
#include <KxCfgHlp.h>
#include <KexGui.h>
#define FRIENDLYAPPNAME_ENG L"VxKex Vista Configuration"
VOID KexCfgOpenGUI(VOID);
DWORD KexCfgApplyGlobalArguments(int Count, PWSTR *Arguments);
DWORD KexCfgApplyBulkArguments(int Count, PWSTR *Arguments);
BOOL KexCfgAppendArgument(PWSTR Command, SIZE_T Capacity, PCWSTR Argument);
DWORD KexCfgRunWriter(HWND Owner, PCWSTR Arguments);
