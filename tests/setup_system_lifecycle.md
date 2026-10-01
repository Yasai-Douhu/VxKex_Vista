# Actual-system setup lifecycle diagnostic

This is a destructive setup test for a **disposable VM only**. It installs and
removes the entire VxKex installation at C:\VxKex, both physical system DLL
deployments, and real machine/user configuration. Do not authorize it on the
working Server 2008 VM. The native probe refuses to write anything unless the
explicit disposable-VM marker is present.

## Verified environment

The independent full clone is:

```text
C:\Users\YamaR\Documents\Virtual Machines\VxKex-Next-Parity-Test\Server2008-SetupParity.vmx
```

It was materialized from the completed `NEXT_Setup_Lifecycle_Source_20261001`
snapshot. The source VM stayed running. Direct cloning of a running VM was
rejected, so a never-booted offline VMX descriptor referenced that snapshot's
closed disk, and vmrun cloned the descriptor to a separate full disk. The result
has `parentCID=ffffffff`, no parentFileNameHint, a local monolithicSparse disk,
CPU 2, RAM 4096 MB and `ethernet0.present=FALSE`. It does not write back to the
original VM's disk. Host provenance is in audit/setup-test-clone.json.
The temporary offline VMX descriptor and its copied NVRAM were removed after
full-clone verification so that descriptor cannot be accidentally booted.

## Preparation and execution

1. Confirm the VMX and backing disk identify this disposable clone, and its NIC
   remains disabled. Copy the complete current Installer input to the clone's
   C:\VxKexProbe\NextParity\RealPackage.
2. Build tests/build_installed_version_probe.ps1 for x64 and x86, and
   tests/build_setup_system_lifecycle_probe.ps1 for x64, using current libraries.
3. Copy the two version probes to Native\VxKexParityRuntime20261001.exe and
   Wow\VxKexParityRuntime20261001.exe under C:\VxKexProbe\NextParity.
   Also copy each architecture to its adjacent VxKexParityDisabled20261001.exe.
4. Copy Installer\VistaSetup.exe and the lifecycle probe outside C:\VxKex.
5. **Only on the verified clone**, create C:\VxKexProbe\NextParity\DisposableVM.txt
   with the exact ASCII bytes, without BOM or newline:
   `VxKex setup lifecycle disposable VM 20261001`.
6. Stop the clone's PostgreSQL service gracefully and close its Explorer before
   installing; those processes may map deployed compatibility files. The
   original VM's applications and services must remain untouched.
7. Execute setup_system_lifecycle_probe_x64.exe as Administrator on the clone.
   A failing stage is retained for analysis; do not repeatedly rerun it without
   inspecting that state. Successful completion removes its own IFEO fixture.

The probe first rejects missing authorization, preexisting fixture keys and a
preexisting preservation store. It invokes the production frontend for install,
keep uninstall, reinstall and remove-all. It inspects real registrations, both
system provider DLL paths, preserved-store persistence and consumption, and
actual x64/x86 child processes before and after restoration.

## Results and limits

The first cycle tested update of the clone's existing installation. The second
cycle tested fresh installation after that cycle's remove-all. Both completed
all four production commits successfully. The strengthened second run was
Failures=0 and independently verified four child reports:

```text
ProcessBits=64 KexDllLoaded=1
GetVersionEx=10.0.19045 RtlGetVersion=10.0.19045 Status=00000000
ProcessBits=32 KexDllLoaded=1
GetVersionEx=10.0.19045 RtlGetVersion=10.0.19045 Status=00000000
```

Each architecture produced those values both after install and after restore.
The child writes an explicit report because the existing IFEO launcher does not
preserve the redirected stdout route used by this harness. A launcher exit code
alone is no longer the only runtime evidence.

The expanded third run completed with Failures=0 (152 log lines). It verified
enabled and disabled profiles in both physical IFEO views. Disabled children
reported KexDllLoaded=0 and both APIs returned 6.0.6003 after installation and
after restoration. Both profiles preserved an unrelated IFEO value and flag
bit. A custom LogDir and actual initiating-user EnableLogging preference
survived keep uninstall and reinstall. Remove-all removed the product preferences
while preserving the unrelated IFEO data and external custom log directory.

Host artifacts are audit/setup-system-lifecycle-initial.txt,
audit/setup-system-lifecycle-enabled-only.txt, audit/setup-system-lifecycle.txt,
lifecycle-stage-0 through -3.txt and
lifecycle-{installed,restored}-{enabled,disabled}-{x64,x86}.report.txt.
The latest stage logs and all eight expanded child reports were retrieved.

## Explicit different-user scope

Build tests/build_setup_user_scope_probe.ps1 for x64 and run its executable
only on the marker-authorized clone after remove-all. This diagnostic creates
one new local standard account, rejects an existing account/profile/operator
preference, loads its real hive and records its actual SID. It uses the
documented LoadUserProfile privileges on the elevated operator, not a fake hive.
The resolved profile directory must exactly match the test's authorized path
before DeleteProfile can run. On failure the new account/profile is retained
for inspection; never blindly rerun into that state.

The test checks that the actual standard token is not an administrator, requires
elevation and cannot open real IFEO for writing. It writes distinct standard-user
and operator preferences, then runs the production frontend as the elevated
operator with the standard user's explicit SID. Install preserves both values;
remove-all deletes only the selected standard user's product preference and
preserves the operator's. It then removes only its own operator preference,
unloads the hive and deletes the verified test profile and newly created account.

The first diagnostic attempt stopped before product setup because name-based
Users membership failed. The created account was inspected and removed with
no profile present. Membership now uses the account SID. The rerun passed:
audit/setup-user-scope.txt has Failures=0; user-scope-install.txt and
user-scope-remove.txt both show successful production commits.

This proves explicit different-user selection and standard-token permissions.
It does not test interactive UAC consent, the ShellExecute elevation handoff or
the interactive install batch under another user's desktop session.

Profile API references: [LoadUserProfile](https://learn.microsoft.com/en-us/windows/win32/api/userenv/nf-userenv-loaduserprofilew),
[GetUserProfileDirectory](https://learn.microsoft.com/en-us/windows/win32/api/userenv/nf-userenv-getuserprofiledirectoryw),
[DeleteProfile](https://learn.microsoft.com/en-us/windows/win32/api/userenv/nf-userenv-deleteprofilew).

This proves representative enabled/disabled restoration, actual provider/API
behavior, real custom/user preference preservation and explicit different-user
scope. The interactive batch menu, Explorer restart recovery, every legacy
profile, actual UAC handoff and automatic cache cleanup still require work.
The clone ends with VxKex uninstalled; its PostgreSQL service stays stopped.
Shut down the test clone gracefully after result collection. The original
Server 2008 installation and running applications remain untouched.
