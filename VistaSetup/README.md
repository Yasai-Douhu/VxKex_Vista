# VistaSetup development frontend

This native x64 frontend runs on Windows Vista / Server 2008 x64. It uses native
Vista registry and file transactions and does not load VxKex DLLs. The working
Installer folder now includes it, and install.bat invokes it through a protected
external cache. This development package has not been published.

## Build

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build_kxcfghlp.ps1 -OutputDirectory audit/build-preservation-engine/KxCfgHlp
powershell -NoProfile -ExecutionPolicy Bypass -File build_vistasetup.ps1 -ConfigurationLibrary audit/build-preservation-engine/KxCfgHlp/KxCfgHlp.lib
```

The output is `audit/VistaSetup.exe`. The version comes from `vautogen.h`, and
the input package's KexCfg.exe must have the same file version. Version matching
does not establish that every package file was built from the same source.

## Commands

```text
VistaSetup --current-user-sid
VistaSetup --is-elevated
VistaSetup --elevate-installer
VistaSetup --prepare-cache <absolute-package-path>
VistaSetup --complete-cache
VistaSetup --install <absolute-package-path> [--user-sid SID]
VistaSetup --check-install <absolute-package-path> [--user-sid SID]
VistaSetup --uninstall-keep [--user-sid SID]
VistaSetup --check-uninstall-keep [--user-sid SID]
VistaSetup --uninstall-remove --user-sid SID
VistaSetup --check-uninstall-remove --user-sid SID
```

The manifest is `asInvoker`. Setup and cache commands require an already elevated
process and NT 6.0 x64. SID reporting and the elevation controls can run before UAC.
The native elevation entry
captures the initiating user's SID before elevation. Its different-administrator
account behavior still requires testing. The frontend opens that explicitly named,
loaded user hive; it does not infer a user from the elevated account. Remove-all
requires this argument. Without it, other modes leave user preferences untouched.

The production destination is C:\VxKex. The batch stages the complete input
package under ProgramData\VxKexVistaSetup\Package-{GUID} before transferring to
Run-VxKexSetup.cmd without CALL. Each run has a separate package directory, so
an earlier helper may remain mapped. The source can be the installed C:\VxKex
tree; deployment uses the separate cached source. Setup runs outside the target
and changes its working directory to Windows before installation or removal.

Cache creation, marker creation and copying use a transaction. Existing roots
must have the ownership marker, Administrators/SYSTEM ownership, and the exact
protected Administrators/SYSTEM full-access DACL. Insecure roots are rejected.
All copied objects are assigned the protected DACL and Administrators ownership
before commit; source and copied critical PE images are checked. Failed staging
rolls back the new package.

After the dispatcher finishes (including a check or failed setup), it records
completion in its own staged directory. The protected record identifies its
parent command process by PID and creation time. On a later successful cache
preparation, completed packages are collected only after that exact process
has exited. A reused PID is distinguished by its creation time. A failed process
query retains the package. The newly staged package is explicitly excluded.
Each removal has its own transaction: a locked payload rolls back the whole
package deletion. Completion records are not inherited when a cached package
is used as input. Unmarked/crashed packages, malformed records and unknown
directory names are retained; the cache root and ownership marker remain.
Collection failure never changes the installation result.

The native process timing API is available on Vista/Server 2008;
see [GetProcessTimes](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocesstimes).

The native elevation launcher captures the original token's SID and passes it
to install.bat. It uses SEE_MASK_NOASYNC because it exits after ShellExecuteEx;
see the [Microsoft Shell execution documentation](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-shellexecuteinfoa).
Different-account UAC testing remains pending. An already elevated caller is
treated as the initiating user unless it explicitly supplies the original SID.

Check-only commands always roll back. Other commands commit only when every
stage succeeds. Errors also roll back, and rollback failures are reported
separately. The console reports the last attempted stage and Win32 status.

## Current verification scope

Server 2008 x64 diagnostic runs reached `deploy-package` (status 32) during
installation and `remove-files` (status 5) during both uninstall modes. These are
failures on the live installation, not successful install/uninstall evidence.
The rollback observer confirmed unchanged registry values and file bytes for:

- Both real IFEO views, VXsoft settings, targeted shell/log registrations.
- The initiating Administrator's VXsoft settings.
- C:\VxKex files and attributes, excluding live Logs.
- The 13 managed DLLs and optional KxSChanl in physical System32 and SysWOW64.

The observer does not compare ACLs, timestamps or concurrently written Logs.
The new batch's `--check-install` path was also run under an elevated command
prompt. It staged a protected package, transferred to the dispatcher, reached
deployment status 32 and returned 32. The observer again confirmed unchanged
real installation state. Check mode does not stop Explorer or invoke UAC.

Dedicated-root integration probes cover successful install, keep uninstall,
reinstall and remove-all, on x64 and x86. The independent disposable full clone
also passed all four production commits against actual system directories,
enabled/disabled x64/x86 runtime verification, custom/user preference preservation
and explicit different-user SID selection. See tests/setup_system_lifecycle.md.
Interactive different-account UAC handoff and Explorer recovery remain pending.

The expanded setup_cache_probe passed on Server 2008: live owner retention,
expired owner collection, locked late payload rollback, explicit keep-path,
foreign name retention and stripping stale completion authorization on restage.
Two actual install.bat --check-install invocations in separate command processes
also passed. The second removed the first completed package, retained its own
completion record and left C:\VxKex absent (check-only rollback). Evidence is in
audit/setup-cache-result.txt and audit/cache-cli-{first,second,verify}.txt.
