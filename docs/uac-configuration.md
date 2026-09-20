# VxKex Vista 1.2.0.2227: UAC configuration saving

## Changes

- Ship the required x64 `KexCfg.exe` in the installer. Previously an unelevated
  property sheet tried to run this missing executable and reported error 2.
- Apply changes through `ShellExecuteEx` with `runas`, wait for completion, and
  report the helper's exit code. The helper requests administrator privileges
  through its embedded manifest and commits configuration in a registry transaction.
- Cancelling UAC keeps the edited settings pending without a second error dialog.
- The installer requires the helper and renames a loaded shell extension before
  replacing it. Restarting Explorer at the end activates the new DLL.

The `runas` elevation mechanism follows Microsoft's documentation:
https://learn.microsoft.com/en-us/windows/win32/shell/launch

## Build (VS2010 x64 and Windows SDK 7.1)

Run `build_kxcfghlp.ps1`, `build_kexcfg.ps1`, and `build_kexshlex.ps1` in that
order. Copy `x64/Release/KexCfg/KexCfg.exe` and
`x64/Release/KexShlEx/KexShlEx.dll` into `Installer`.
Both changed executables have file version 1.2.0.2227, matching release tag
`v1.2.0.2227`. Unchanged compatibility DLLs retain their existing file versions.

## Validation status

The x64 build and PE import/resource inspection passed. The helper has no
MSVC runtime redistributable or VxKex DLL dependency. No interactive UAC or
application launch test was performed for this release; user validation is pending.

On the Server 2008 VM the updated files are deployed in `C:\VxKex`.
Close property dialogs and restart Explorer (or sign out and back in) before
checking the new extension. The previous extension is retained as
`KexShlEx.pre-uac-2227.dll` for this deployment.

Manual checks from an unelevated Explorer:

1. Change version spoofing, choose Apply, accept UAC, and reopen Properties to
   check that the values persisted. Check every option and disabling VxKex.
2. Cancel UAC: Properties must stay open with the edits still pending. Retry
   Apply and accept UAC, then check the persisted values.
3. Launch VS Code by double-click with VxKex enabled.
4. If saving fails, note the displayed error number. A failed helper transaction
   must not leave partially updated settings.

UAC prompts depend on Windows policy and the caller's existing elevation.
Already elevated callers can save directly without another prompt.
