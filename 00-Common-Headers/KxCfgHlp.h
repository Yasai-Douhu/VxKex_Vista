///////////////////////////////////////////////////////////////////////////////
//
// Module Name:
//
//     KxCfgHlp.h
//
// Abstract:
//
//     Main header file for the VxKex Configuration Helper library.
//
// Author:
//
//     vxiiduu (02-Feb-2024)
//
// Environment:
//
//     Win32 mode.
//
// Revision History:
//
//     vxiiduu              02-Feb-2024  Initial creation.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include <KexComm.h>
#include <KexDll.h>
#include <KexW32ML.h>

#ifdef KEX_ENV_NATIVE
#  error This header file cannot be used in a native mode project.
#endif

#ifndef KXCFGDECLSPEC
#  define KXCFGDECLSPEC
#endif

#define KXCFGAPI WINAPI

#define KXCFG_ELEVATION_SCHTASK_NAME L"VxKex Configuration Elevation Task"

//
// Type definitions
//

typedef struct {
	BOOLEAN				Enabled;
	BOOLEAN				DisableForChild;
	BOOLEAN				DisableAppSpecificHacks;
	KEX_WIN_VER_SPOOF	WinVerSpoof;
	ULONG				StrongSpoofOptions;
} TYPEDEF_TYPE_NAME(KXCFG_PROGRAM_CONFIGURATION);

//
// ExeFullPathOrBaseName can be either a full path (C:\folder\program.exe)
// or a basename (program.exe). The callee can determine this easily by
// checking IsLegacyConfiguration (TRUE means it's a base name only).
//
// This function should return TRUE to continue enumeration or FALSE to stop
// enumerating.
//
typedef BOOLEAN (CALLBACK *PKXCFG_ENUMERATE_CONFIGURATION_CALLBACK) (
	IN	PCWSTR							ExeFullPathOrBaseName,
	IN	BOOLEAN							IsLegacyConfiguration,
	IN	PVOID							ExtraParameter);

//
// Public functions
//

KXCFGDECLSPEC HKEY KXCFGAPI KxCfgOpenVxKexRegistryKey(
	IN	BOOLEAN		PerUserKey,
	IN	ACCESS_MASK	DesiredAccess,
	IN	HANDLE		TransactionHandle OPTIONAL);

KXCFGDECLSPEC HKEY KXCFGAPI KxCfgOpenLegacyVxKexRegistryKey(
	IN	BOOLEAN		PerUserKey,
	IN	ACCESS_MASK	DesiredAccess,
	IN	HANDLE		TransactionHandle OPTIONAL);

KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgGetConfiguration(
	IN	PCWSTR							ExeFullPath,
	OUT	PKXCFG_PROGRAM_CONFIGURATION	Configuration);

KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgEnumerateConfiguration(
	IN	PKXCFG_ENUMERATE_CONFIGURATION_CALLBACK	ConfigurationCallback,
	IN	PVOID									CallbackExtraParameter);

KXCFGDECLSPEC BOOLEAN KxCfgSetConfiguration(
	IN	PCWSTR							ExeFullPath,
	IN	PKXCFG_PROGRAM_CONFIGURATION	Configuration,
	IN	HANDLE							TransactionHandle OPTIONAL);

KXCFGDECLSPEC BOOLEAN KxCfgDeleteConfiguration(
	IN	PCWSTR	ExeFullPath,
	IN	HANDLE	TransactionHandle OPTIONAL);

KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgDeleteLegacyConfiguration(
	IN	PCWSTR	ExeFullPath,
	IN	HANDLE	TransactionHandle OPTIONAL);

KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgGetKexDir(
	OUT	PWSTR	Buffer,
	IN	ULONG	BufferCch);

KXCFGDECLSPEC BOOLEAN WINAPI KxCfgEnableVxKexForMsiexec(
	IN	BOOLEAN	Enable,
	IN	HANDLE	TransactionHandle OPTIONAL);

KXCFGDECLSPEC BOOLEAN WINAPI KxCfgQueryVxKexEnabledForMsiexec(
	VOID);

KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgEnableExplorerCpiwBypass(
	IN	BOOLEAN	Enable,
	IN	HANDLE	TransactionHandle OPTIONAL);

KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgQueryExplorerCpiwBypass(
	VOID);

KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgQueryUnstableExplorerCpiwBypass(
	VOID);

KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgEnableUnstableExplorerCpiwBypass(
	IN	BOOLEAN	Enable,
	IN	HANDLE	TransactionHandle OPTIONAL);

KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgQueryShellContextMenuEntries(
	OUT	PBOOLEAN	ExtendedMenu OPTIONAL);

KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgConfigureShellContextMenuEntries(
	IN	BOOLEAN	Enable,
	IN	BOOLEAN	ExtendedMenu,
	IN	HANDLE	TransactionHandle OPTIONAL);

KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgQueryLoggingSettings(
	OUT	PBOOLEAN	IsEnabled OPTIONAL,
	OUT	PWSTR		LogDir OPTIONAL,
	IN	ULONG		LogDirCch);

KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgConfigureLoggingSettings(
	IN	BOOLEAN		Enabled,
	IN	PCWSTR		LogDir OPTIONAL,
	IN	HANDLE		TransactionHandle OPTIONAL);

KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgInstallDiskCleanupHandler(
	IN	PCWSTR	KexDir OPTIONAL,
	IN	PCWSTR	LogDir OPTIONAL,
	IN	HANDLE	TransactionHandle OPTIONAL);

KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgRemoveDiskCleanupHandler(
	IN	HANDLE	TransactionHandle OPTIONAL);

KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgQueryLegacyKxSChanlSsp(
	VOID);

KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgEnableLegacyKxSChanlSsp(
	IN	BOOLEAN	Enable,
	IN	HANDLE	TransactionHandle OPTIONAL);

#ifdef KXCFGDECLSPEC
//
// Private functions
//

BOOLEAN KxCfgpRemoveKexDllFromVerifierDlls(
	IN	PWSTR	VerifierDlls);

REGSAM KxCfgpIfeoView(PCWSTR ExeFullPath);
NTSTATUS KxCfgpOpenIfeoKey(PCWSTR ExeFullPath, PHKEY KeyHandle);

BOOLEAN KxCfgpCreateIfeoKeyForProgram(
	IN	PCWSTR	ExeFullPath,
	OUT	PHKEY	KeyHandle,
	IN	HANDLE	TransactionHandle OPTIONAL);

BOOLEAN KxCfgpElevationRequired(
	VOID);

BOOLEAN KxCfgpElevatedSetConfiguration(
	IN	PCWSTR							ExeFullPath,
	IN	PKXCFG_PROGRAM_CONFIGURATION	Configuration);

BOOLEAN KxCfgpElevatedDeleteConfiguration(
	IN	PCWSTR							ExeFullPath);

BOOLEAN KxCfgpElevatedSetConfigurationTaskScheduler(
	IN	PCWSTR							ExeFullPath,
	IN	PKXCFG_PROGRAM_CONFIGURATION	Configuration);

BOOLEAN KxCfgpElevatedSetConfigurationShellExecute(
	IN	PCWSTR							ExeFullPath,
	IN	PKXCFG_PROGRAM_CONFIGURATION	Configuration);

BOOLEAN KxCfgpAssembleKexCfgCommandLine(
	OUT	PWSTR							Buffer,
	IN	ULONG							BufferCch,
	IN	PCWSTR							ExeFullPath,
	IN	PKXCFG_PROGRAM_CONFIGURATION	Configuration);

HKEY KxCfgpCreateKey(
	IN	HKEY		RootDirectory,
	IN	PCWSTR		KeyPath,
	IN	ACCESS_MASK	DesiredAccess,
	IN	HANDLE		TransactionHandle OPTIONAL);

HKEY KxCfgpOpenKey(
	IN	HKEY		RootDirectory,
	IN	PCWSTR		KeyPath,
	IN	ACCESS_MASK	DesiredAccess,
	IN	HANDLE		TransactionHandle OPTIONAL);

ULONG KxCfgpDeleteKey(
	IN	HKEY	KeyHandle,
	IN	PCWSTR	KeyPath OPTIONAL,
	IN	HANDLE	TransactionHandle OPTIONAL);

#endif

// Internal preservation primitives: both keys must belong to the same transaction.
INT NTAPI RtlOperatingSystemBitness(VOID);
LONG KxCfgpPreserveIfeoConfiguration(HKEY Source, HKEY Backup);
LONG KxCfgpRestoreIfeoConfiguration(HKEY Source, HKEY Backup);
LONG KxCfgpPreserveIfeoView(HKEY IfeoRoot, HKEY Backup, HANDLE Transaction, REGSAM View);
LONG KxCfgpRestoreIfeoView(HKEY IfeoRoot, HKEY Backup, HANDLE Transaction, REGSAM View);
// Required transaction: the caller commits only after the whole operation
// succeeds and rolls back on every failure. Restore consumes the store.
KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgPreserveAllConfigurations(HANDLE Transaction);
KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgRestoreAllConfigurations(HANDLE Transaction);
KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgPrepareUninstall(BOOLEAN KeepSettings, HANDLE Transaction);
// Native TxF primitives. Roots must be authorized by the caller; on any error
// roll back the shared registry/filesystem transaction. Never commit here.
LONG KxCfgpCopySetupFile(PCWSTR Source, PCWSTR Destination, HANDLE Transaction);
LONG KxCfgpDeleteSetupFile(PCWSTR Path, HANDLE Transaction);
LONG KxCfgpCopySetupDirectory(PCWSTR Source, PCWSTR Destination, HANDLE Transaction);
LONG KxCfgpRemoveSetupDirectory(PCWSTR Path, HANDLE Transaction);
// Shared with VxlView and setup: caller commits or rolls back the whole change.
LONG KxCfgpUpdateLogAssociation(HKEY ClassesRoot, PCWSTR Executable,
    BOOLEAN Install, HANDLE Transaction);
LONG KxCfgpUpdateShellExtension(HKEY ClassesRoot, HKEY SoftwareRoot,
    PCWSTR DllPath, BOOLEAN Install, HANDLE Transaction);
// Caller authorizes all roots and uses native system paths (native setup process).
// These stage file operations only; caller also stages registry configuration.
LONG KxCfgpDeploySetupFiles(PCWSTR Package, PCWSTR Target, PCWSTR NativeSystem,
    PCWSTR WowSystem, HANDLE Transaction);
LONG KxCfgpUpdateContextMenu(HKEY ClassesRoot, PCWSTR KexDir, PCWSTR WindowsDirectory,
    BOOLEAN Enable, BOOLEAN ExtendedMenu, HANDLE Transaction);
LONG KxCfgpStageLoggingSettings(HKEY UserSoftware, HKEY MachineSoftware,
    PCWSTR KexDir, BOOLEAN Enabled, PCWSTR ResolvedLogDir, HANDLE Transaction);

LONG KxCfgpRemoveSetupFiles(PCWSTR Target, PCWSTR NativeSystem,
    PCWSTR WowSystem, HANDLE Transaction);
LONG KxCfgpValidateSetupBinary(PCWSTR Path, WORD Machine, BOOLEAN Dll, HANDLE Transaction);
LONG KxCfgpValidateSetupPackage(PCWSTR Package, HANDLE Transaction);
LONG KxCfgpDeploySetupPackage(PCWSTR Package, PCWSTR Target, PCWSTR NativeSystem,
    PCWSTR WowSystem, HANDLE Transaction);
LONG KxCfgpConfigureSetupSettings(HKEY MachineSoftware, HKEY UserSoftware,
    PCWSTR Target, DWORD Version, BOOLEAN Install, BOOLEAN KeepSettings, HANDLE Transaction);
LONG KxCfgpConfigurePropagationTemplate(HKEY IfeoRoot, REGSAM View,
    BOOLEAN Install, HANDLE Transaction);
LONG KxCfgpProcessConfigurationRoots(BOOLEAN Restore, HANDLE Transaction,
    HKEY StoreParent, PCWSTR StorePath, HKEY NativeIfeo, HKEY WowIfeo);

typedef struct _KXCFG_SETUP_CONTEXT {
    DWORD Size;
    PCWSTR Package, Target, NativeSystem, WowSystem;
    HKEY MachineSoftware, UserSoftware, ClassesRoot, NativeIfeo, WowIfeo;
    DWORD InstalledVersion;
    PCWSTR StageName; // Output: last operation attempted, for error reporting.
} KXCFG_SETUP_CONTEXT, *PKXCFG_SETUP_CONTEXT;
// Roots must be authorized by the front end. This stage never commits.
LONG KxCfgpStageSetup(PKXCFG_SETUP_CONTEXT Context, BOOLEAN Install,
    BOOLEAN KeepSettings, HANDLE Transaction);
