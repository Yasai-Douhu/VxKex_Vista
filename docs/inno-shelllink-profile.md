# Inno Setup ShellLink creation on NT 6.0 (2026-09-28)

The reported CLSID `{00021401-0000-0000-C000-000000000046}` is ShellLink,
not a missing known-folder identifier. Native Server 2008 can create and save
this COM object. Calling `SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32)`
first makes `CoCreateInstance` fail with `E_INVALIDARG` (`0x80070057`).

Inno Setup 6.0.5 explicitly avoids that call on Vista, but version spoofing
bypasses its version check. Newer Inno releases also removed the old fallback.
Sources:

- [Inno 6.0.5 SafeDLLPath](https://github.com/jrsoftware/issrc/blob/is-6_0_5/Projects/SafeDLLPath.pas)
- [Inno 6.5.4 SafeDLLPath](https://github.com/jrsoftware/issrc/blob/is-6_5_4/Components/SafeDLLPath.pas)
- [Microsoft SetDefaultDllDirectories documentation](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-setdefaultdlldirectories)

## Implementation

The existing x86 Inno resource-attribution detector now sets
`KEXDATA_FLAG_INNO_SETUP` before searching for COMCTL32 import thunks.
Detection therefore also works when that control initialization layout differs.
The existing real-NT-6.0 and `DisableAppSpecific` gates still apply.

For these processes, KxBase handles the exact SYSTEM32-only policy request by
removing the current directory with `SetDllDirectoryW(L"")` and preloading
Inno's legacy list of system DLLs by absolute system-directory paths. It returns
`FALSE`/`ERROR_NOT_SUPPORTED`: the requested modern policy was not installed.
The fallback is implemented in VxKex so newer engines also benefit. It is the
legacy Inno mitigation, not an emulation of a SYSTEM32-only DLL search policy.

`Stub_GetProcAddress` also routes this one lookup on native KERNEL32 through
the wrapper. An actual Inno installation test exposed that its dynamic lookup
can otherwise bypass the wrapper. Other API names, module handles, ordinal
lookups and non-Inno processes retain the existing behavior.

## Verification on Server 2008 x64

- Native x86 probe without a search-policy change: COM creation and `.lnk`
  save both return `S_OK`.
- Native x86 probe after the policy change: COM creation returns `0x80070057`.
- VxKex-enabled probe carrying the resource marker: the policy request returns
  error 50, then COM creation and save return `S_OK`.
- An unmarked probe and a marked probe with app-specific workarounds disabled
  retain native behavior and reproduce the error.
- A minimal installer compiled with official Inno Setup 6.0.5 reproduces the
  exact ClassID error with the profile disabled. With it enabled, its log says
  `Successfully created the icon.` and the resulting shortcut targets
  `C:\Windows\System32\notepad.exe`. No product payload is installed.
- WOW64 parent/child propagation and version reporting still pass with both
  exit codes zero and child version `10.0.19045`.
- VS2010 x86 builds and `git diff --check` pass.

The suppressed-error Inno test exits zero even when shortcut creation fails;
the installer log and saved shortcut, not exit code alone, are the assertions.

Reproduction sources: `tests/vista_shelllink_probe.cpp`, its optional synthetic
resource `tests/vista_shelllink_probe.rc`, and `tests/inno_shelllink_probe.iss`.
Compile the C++ probe using VS2010 `/MT` with SDK `ole32.lib` and `uuid.lib`.
Run each mode in a fresh process; run `native`/`none` without VxKex first.
Enable VxKex for marked/unmarked copies when testing `compat`. The `.iss` fixture
creates only a diagnostic shortcut under `C:\VxKexProbe\ShellLinkInstaller`,
without an uninstaller or installed-program registration.

Local evidence is in `audit/shelllink-*.txt` and `audit/shelllink-setup-*.log`.
The final `.iss` fixture has no diagnostic DLL imports.

## Deployment and limits

Updated x86 KexDll and KxBase are deployed to the VM's `C:\Windows\SysWOW64`
and `C:\VxKex\Kex32`, and copied into repository `Installer\Kex32`.
Original VM files have the suffix `.pre-shelllink-20260928` for rollback.
The SysWOW64 copies were retrieved and their hashes matched the package files:

| File | SHA256 |
| --- | --- |
| KexDll.dll | `1674DA428EDC645CAE63DF3E5851D14A56F1B5CD49FBAED1C130581120BF85F4` |
| KxBase.dll | `3416322DEBFCD321ADAFEF3B950564563EED8BAE838D279B15144CB093108DCF` |

Restart the installer from the beginning: a process which already installed
the failing DLL search policy cannot undo it merely by retrying the dialog.
This covers the existing x86 Inno identification profile on real NT 6.0;
native x64 setup engines or modified/absent attribution resources are outside
its detection scope. Full product installations of Sublime/VS Code/Git were
not rerun; the command-driven minimal installer and COM tests were used instead.
No release upload or commit was performed for this fix.
