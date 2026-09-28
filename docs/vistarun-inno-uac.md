# IFEO launcher lifetime and Inno Setup UAC on Vista

On Vista, Git for Windows 2.55.0.5 with VxKex enabled could show
`CallSpawnServer: Unexpected response: $0` when started with a filtered token
and elevated through UAC. Starting it elevated initially avoided the error.

The Inno Setup 6.5.4 [spawn server code](https://github.com/jrsoftware/issrc/blob/is-6_5_4/Projects/Src/Setup.SpawnServer.pas)
creates a message-only server window in the original process, then uses
`ShellExecuteEx("runas")` with `SEE_MASK_NOCLOSEPROCESS`. It waits for the
returned process handle before destroying the server window. The elevated
client later sends `WM_COPYDATA` to that window, and reports `Unexpected
response: $0` if the window returns zero. The [client code](https://github.com/jrsoftware/issrc/blob/is-6_5_4/Projects/Src/Setup.SpawnClient.pas)
shows this check.

The IFEO Debugger entry points to `VistaRun.exe`. Previously VistaRun exited
as soon as it created and detached from the real target, so the handle
returned by ShellExecuteEx could become signaled while the elevated installer
was still running. Inno then destroyed the original process's spawn server
window. VistaRun now waits for the real target and returns its exit code.

## Verification

On the Vista VM, a small IFEO child-process probe exposed the previous bug:
the old launcher returned `0` while its target later returned `37`. With the
new launcher, the handle stayed live and returned `37`. The same result was
observed after copying the new binary to `C:\VxKex\VistaRun.exe`.
The packaged `Installer/VistaRun.exe` and the VM copy have matching SHA256
`42CE7C8B3F1DDCF728AF2E91EAFA3974B799E573349E91AEFCA7118C103CAFFC`.

The same launcher lifetime probe also passed on Server 2008: the initial
500 ms wait timed out and the eventual process exit code was 37.

The user's subsequent interactive UAC retest displayed no setup window.
Live debugging identified a second problem below. The account named Vista
belongs to Administrators; the relevant distinction is its filtered versus
elevated token, rather than the account name or group membership alone.

The launcher remains present for as long as the target application runs.
This is necessary for callers that wait on the IFEO debugger process handle.

## Frozen tick count under strong version spoofing

The second launch retained its original spawn server (PID 4664, HWND 0x40316).
The elevated installer (PID 5088) repeatedly waited for 10 ms inside
`CallSpawnServer`. Sending its status query, message 0x1950, wParam 1,
sequence 1, returned 0x6c830003 (`RETURNED_TRUE`), including from the elevated
token. The server had already replied; this was not a failed UAC message.

Inno only checks that status after `GetTickCount() - LastQueryTime >= 10`.
The tick count at SharedUserData+0x320 remained unchanged across debugger
samples. Strong version spoofing copies the SharedUserData page on write,
freezing the kernel-updated time fields. Existing hooks repaired system time
but omitted GetTickCount and GetTickCount64. A 32-bit probe reproduced a zero
delta even after Sleep(250).

KexDll now records the boot-relative tick value and native performance
counter before copying the page. KxBase hooks both tick-count APIs and uses
the counter's elapsed milliseconds, preserving the boot offset and 32-bit
wrap. The native syscall avoids the frozen page. A failed initial counter
query skips SharedUserData spoofing rather than freezing the clock.

These APIs already exist on Vista. See Microsoft's
[GetTickCount64 documentation](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-gettickcount64)
and [performance counter guidance](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps).
QPC is independent of wall-clock adjustments and has a frequency fixed at boot.
The derived timer has finer resolution than the native tick; it is an
approximation anchored to the original boot-relative value, not a claim
that the two clocks are identical.

After deployment, `tests/strong_spoof_tick_probe.c` passed on Vista with
strong spoofing enabled: 32-bit delta 267 ms, 64-bit delta 266 ms, no samples
below the initial value, consistent low 32 bits, and unchanged LastError (0x12345678).
Both KexDll and KxBase were loaded. Temporary probe IFEO keys were removed.
The four deployed DLLs have `.pre-tick-20260928` backups alongside their
original locations. Installer copies were updated for both architectures.

The probe also passed on both architectures with StrongVersionSpoof 0 and 1
(264-265 ms). On 2026-09-28 the user confirmed that starting the Git installer
normally and accepting UAC now opens the setup window. This verifies the
reported launch failure; installation to completion was not part of this test.
The temporary VxKexSessionInspect diagnostic scheduled task was removed.
