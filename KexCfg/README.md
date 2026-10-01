# KexCfg development frontend

The NEXT settings GUI is ported with an `asInvoker` manifest. Startup no longer
automatically elevates. Add, delete, clean and global settings use the installed
native writer through `ShellExecuteEx("runas")`; the client waits for its actual
exit code. Only successful saves clear pending changes or refresh the list.
UAC cancellation retains edits without showing a second error dialog. Interactive
UAC handoff and GUI action validation remain unverified; this is a development
build and has not replaced Installer/KexCfg.exe.

## Atomic global settings CLI

The native x64 helper accepts all six required fields, in any order:

```text
KexCfg /GLOBAL /USER-SID:<initiating-user-SID> /LOGGING:0|1 /LOGDIR:<resolved-absolute-path> /MSI:0|1 /CONTEXTMENU:0|1 /EXTENDED:0|1
KexCfg /CHECKGLOBAL /USER-SID:<initiating-user-SID> /LOGGING:0|1 /LOGDIR:<resolved-absolute-path> /MSI:0|1 /CONTEXTMENU:0|1 /EXTENDED:0|1
```

Quote each entire argument when its value contains spaces. Duplicate fields,
missing fields, unknown arguments, relative directories and non-boolean toggles
are rejected. A real loaded local/domain user hive is required. Service/system
SIDs are rejected. The invoking GUI must capture its own SID and expand the log
path **before** requesting elevation; the helper does not expand the directory
with another administrator's environment.

The command requires elevation and never recursively elevates itself. It writes
logging preferences into the explicit user's Software hive, updates the disk
cleanup registration, manages both physical native/WOW64 msiexec configurations
and updates machine EXE/MSI context menus in one transaction. Any error rolls
back all preceding changes. `/CHECKGLOBAL` always rolls back. The x86 command
currently returns `ERROR_NOT_SUPPORTED`; the x86 GUI client uses the installed
native x64 writer.

## Verification

Dedicated-root logging tests passed in x64 and x86 on Server 2008, including
explicit user selection, no operator HKCU mutation, rollback and cleanup-owner
conflict. Evidence: audit/logging-settings-explicit-{x64,x86}.txt.

The disposable-clone test `global_settings_cli_probe.c` executes the actual
native CLI against real machine/user roots. Check-only, commit, both MSI views,
late MSI-menu conflict rollback and invalid system SID passed (`Failures=0`).
The initial test assumed enabled logging and failed; it now records and compares
the actual initial configuration rather than inventing a default. Both results
are preserved in audit/global-settings-cli*.txt.

The expanded `setup_user_scope_probe.c` creates a new standard user, loads its
real hive and applies the actual global CLI as Administrator with that user's
SID. It confirms that the selected user receives the new preferences, the
operator's directory preference stays absent, and the standard identity itself
can read the newly saved settings. Evidence:
audit/global-settings-real-user-scope.txt (`Failures=0`).

## Bulk application writer

`/ADD`, `/DELETE` and `/CLEAN` accept one or more quoted absolute image paths.
All arguments are validated before any write. The elevated writer applies the
whole request in one transaction and returns a Win32 error on failure. It does
not invoke per-image elevation. `/CLEAN` rechecks each image after elevation and
removes only missing files on fixed drives; existing files, network paths and
access-denied results retain their profiles.

The disposable-clone CLI test now checks two actual profiles, invalid later
arguments without earlier mutations, cleanup of a mixed existing/missing pair,
and multi-profile deletion. The expanded global-settings regression also passes.
Evidence: `audit/bulk-global-settings-cli.txt` (`Failures=0`). The later
`audit/bulk-runtime-rollback.txt` test modifies only its owned second image to
require a newer subsystem and injects a foreign debugger. The actual bulk writer
returns error 183, rolls back the first image's profile creation, and preserves
the foreign debugger. Removing the owned conflict permits a successful retry;
all fixtures are removed (`Failures=0`). Access-denied/network cleanup branches
still need dedicated runtime coverage.

`audit/gui-writer-global-cli.txt` verifies the production client quoting function
with spaces, Unicode, embedded quotes, trailing backslashes and empty arguments;
overflow is rejected. The actual installed writer's validation error 87 reaches
the client. Global/bulk regression checks also pass (`Failures=0`).

`audit/gui-writer-standard-user-isolated.txt` launches the actual installed
asInvoker CLI under a real standard primary token on a private test desktop.
The unelevated write returns error 5 and retains preferences. An elevated write
then changes only the selected user's settings, which that user can read.
All fixtures were removed (`Failures=0`). The first attempt inherited the
operator's desktop, inaccessible to the network-logon token, and exited with
0xC0000142 before entering the application. That failure and explicit recovery
are preserved in gui-writer-standard-user.txt and gui-writer-scope-recovery.txt.

These tests do not prove interactive GUI save actions, UAC handoff,
cancellation handling or the complete high-priority
NEXT parity goal. The new KexCfg binary is diagnostic-only and has not replaced
Installer/KexCfg.exe.

## Standard-user GUI reader verification

`audit/gui-standard-reader-both.txt` records the actual native and WOW64 GUI
builds running under a real standard primary token on an isolated test desktop.
Both create the NEXT dialog with all 15 required controls, display the standard
user's enabled logging preference while the administrator preference remains
disabled, disable unsupported Explorer BHO and unchanged Apply, and exit through
Cancel without saving. The later elevated writer and user-scope cleanup checks
also pass (`Failures=0`). The isolated desktop is never switched onto the user's
screen. This test does not exercise a save, file picker or UAC prompt.

The x86 helper library was rebuilt from current sources into
`audit/build-gui-reader-x86/KxCfgHlp` and linked into the tested GUI in
`audit/build-gui-reader-x86/KexCfg`. `build_kxcfghlp_x86.ps1 -OutputDirectory`
allows these diagnostic builds without replacing existing release libraries.

## Actual GUI save verification

`audit/gui-save-both-production.txt` exercises both GUI architectures with the
already elevated operator on the isolated desktop. The test enables logging,
edits the actual text buffer through `EM_REPLACESEL`, queues Apply through the
message loop, and verifies the persisted expanded directory containing Japanese
characters and a trailing backslash. Both clear pending changes, disable Apply,
and exit without an unsaved-changes prompt. The x86 GUI invokes the installed
native writer successfully. Preferences and all test fixtures are then restored
and removed (`Failures=0`). This does not prove a standard-user UAC prompt.

Earlier attempts using cross-process `WM_SETTEXT` observed the changed cached
caption but the GUI saved the original edit-buffer contents on this isolated
desktop. These failed results are retained in gui-save-both.txt,
gui-save-values.txt and gui-save-posted.txt. A temporary diagnostic build recorded
the actual writer command; after `EM_REPLACESEL` it contained the correct expanded
Unicode directory (gui-save-command-edited.txt, UTF-16LE). That instrumentation
was removed from source, and the final pass uses the regular build. This was a
test-input discrepancy, not evidence of a directory-save defect in the regular
GUI path.

## Actual GUI failure and retry verification

`audit/gui-save-failure-retry-both-v2.txt` exercises a late foreign MSI context
menu conflict from each actual GUI architecture. The writer rolls back the
pending logging/directory change and the earlier EXE menu creation. The GUI
shows an owned error dialog, retains enabled Apply and pending input, and becomes
usable after the diagnostic dialog is dismissed. Removing only the injected
conflict and retrying Apply without reentering fields saves the pending checkbox
and directory; Apply then disables. All fixtures are removed (`Failures=0`).

The first diagnostic required a physical `IDOK` control in the TaskDialog window
and did not find it, although rollback passed. The corrected probe identifies
the error by its exact test process, parent dialog and window class and uses the
TaskDialog `TDM_CLICK_BUTTON` message. The failure and guarded cleanup are kept
in gui-save-failure-retry-both.txt and gui-failure-recovery.txt. This verification
does not stand in for standard-user UAC approval or cancellation.
