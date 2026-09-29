# VS Code terminal recovery on NT 6.0

Investigation: 2026-09-29. Subsequent implementation restored cmd terminals and
shell tasks in VS Code 1.139.1 on Server 2008. See
[VistaPty implementation and validation](../tools/VistaPty/README.md).
The findings below describe the original feasibility investigation.

## Observed environment

The active Server 2008 application is VS Code **1.139.1**, Electron **43.6.0**:
`C:\Users\Administrator\AppData\Local\Programs\Microsoft VS Code\Code.exe`.
Its app directory is `04c0d99f4f\resources\app` under that installation.
The package declares node-pty `^1.2.0-beta.15`. The unpacked native directory
contains conpty.node, conpty_console_list.node, conpty.dll and OpenConsole.exe;
it does not contain the former WinPTY binaries.

The installed version/path was read from the live VM, not assumed from older
investigations of `C:\vscode`. The editor was left running.

## Why the setting is insufficient

[VS Code 1.109](https://code.visualstudio.com/updates/v1_109#_removal-of-winpty-support)
removed the WinPTY backend. A setting cannot select code which is no longer
shipped. Selecting the bundled ConPTY DLL also does not supply all OS facilities.

The [ConPTY implementation](https://github.com/microsoft/terminal/blob/main/src/winconpty/winconpty.cpp)
creates a console-driver server before starting its console host.
[DeviceHandle](https://github.com/microsoft/terminal/blob/main/src/server/DeviceHandle.cpp)
opens `\Device\ConDrv\Server`. Earlier VM inspection found neither the ConDrv
service nor its driver. NT 6.0 uses the older CSRSS console architecture.

The [node-pty native implementation](https://github.com/microsoft/node-pty/blob/main/src/win/conpty.cc)
also attaches the pseudo console during child-process creation. Consequently,
implementing only CreatePseudoConsole/ResizePseudoConsole/ClosePseudoConsole,
or returning success from unsupported process attributes, is insufficient.
The child must really have a console with working input, output and lifetime.

An isolated x64 probe with VxKex enabled found no kernel32 CreatePseudoConsole
export. Loading this installation's bundled conpty.dll failed with Win32 error
127 (missing procedure). This is a **standalone-probe result**, not the precise
failure observed inside the already-running Electron pty host. Its exact failed
import/HRESULT remains untraced. Fixing that loader failure alone would still
leave the console-driver and process-attachment requirements above.

A direct ELECTRON_RUN_AS_NODE diagnostic did not produce its script log through
the current launcher. It is not counted as a successful node-pty reproduction.
Future integration tests should use a controlled extension-host/pty-host harness
or a launcher path that handles Node mode explicitly.

## New VM evidence: WinPTY works

Built `tests/vista_terminal_backend_probe.cpp` as an x64 VS2010 /MT executable.
It dynamically loads WinPTY, creates an 80x25 console, starts native cmd.exe,
resizes to 100x30, sends a command through the input pipe, captures the VT output,
and checks the requested exit code 7. No direct UI operation was used.

| Check on Server 2008 | Native | VxKex enabled |
|---|---|---|
| WinPTY DLL and agent startup | Pass | Pass |
| cmd.exe startup | Pass | Pass |
| Input and echoed command result | Pass | Pass |
| Resize call | Pass | Pass |
| Child exit notification/code 7 | Pass | Pass |

Both logs end with `PASS=1`. Resize success is an API-level assertion, not a
visual verification of terminal layout. The tests do not yet cover Ctrl+C,
Japanese/IME, fullscreen applications, PowerShell, tasks, debuggers or shell
integration. Vista itself was not retested in this investigation.

Test binaries came from the already-extracted VS Code 1.70.2 distribution:

| File | SHA-256 |
|---|---|
| winpty.dll | `41095A39C0CB721453AC79FDD83D1509876A10BD5F2FF53ADD9A74764CDCA7F1` |
| winpty-agent.exe | `18C00539B806F469EC3623BAA13F5865CC4044F662B2C201B5A3C4D9F5B8F278` |
| Current bundled conpty.dll | `F4E0E92B065DE45CF5F364C37D621B9CD7E4C52619C5F09E65AD58D62DFD8D14` |

The [WinPTY header and constants](https://github.com/rprichard/winpty/tree/master/src/include)
were downloaded to `audit/terminal-research`. Build with that include directory
and VS2010 SDK 7.1; no WinPTY import library is needed. Preserve the upstream MIT
license when packaging its code/binaries. The old binaries are feasibility
fixtures, not a claim that an unmodified old node addon can replace the new one.

Local evidence: `audit/terminal-inventory.txt`, and
`audit/terminal-research/{winpty-native.txt,winpty-kex.txt,conpty.txt,imports.txt}`.
Guest diagnostics are under `C:\VxKexProbe\Terminal`. Temporary probe IFEO
registration was removed by the test script. No production VxKex/VS Code binary
or terminal setting was changed during this investigation.

## Approaches and recommendation

| Approach | Scope and assessment |
|---|---|
| **WinPTY adapter matching node-pty's native interface** | Recommended for ordinary integrated terminals. Retain VS Code's terminal UI and existing JS lifecycle; replace the native backend for supported app versions. Native feasibility is now demonstrated, integration is not. |
| Extension Pseudoterminal + WinPTY helper | Useful smaller prototype and fallback. Avoids patching the editor/native addon, but ordinary shell tasks and debugger terminal requests do not automatically route through it. |
| Transparent ConPTY emulation in VxKex | Possible design direction, substantially wider scope: process attributes, process creation, pseudo-console handle ownership, pipe forwarding, resize and close semantics. A replacement conpty.dll alone is insufficient. |
| Port genuine ConDrv/OpenConsole behavior | Much larger kernel/console compatibility project. Not the first approach for restoring this IDE feature. |

### Recommended native adapter design

Build a new Node-API addon with the installed node-pty's verified native contract,
backed by WinPTY. Do not copy an old pty.node built for a different Electron/V8
ABI. [Node-API](https://nodejs.org/api/n-api.html#implications-of-abi-stability)
reduces runtime ABI coupling; the JS/native contract and supported Node-API
version still must be checked against this installation.

Proposed mapping, to validate against the shipped package before implementation:

- `startProcess`: create a WinPTY session and return its input/output pipe names
  and a tracked session ID; do not start the shell prematurely.
- `connect`: spawn with the exact command line, environment and working directory;
  return the real PID and deliver exit callbacks safely on the JS thread.
- `resize`: forward to winpty_set_size, including after rapid UI resizing.
- `kill`: close the session and handle its descendants without touching unrelated
  processes; coordinate pipe EOF, output draining and the exit callback.
- `clear`: implement an explicitly defined terminal-clearing behavior; do not
  silently advertise unsupported semantics.

The [current JS agent](https://github.com/microsoft/node-pty/blob/main/src/windowsPtyAgent.ts)
uses separate input/output pipes and an output worker. Their readiness and
shutdown ordering must remain compatible to avoid deadlocking the pty host.
The repository's current main is architectural reference material, not proof
that every signature matches the installed beta.

For VxKex distribution, this is an optional **application-specific companion**,
not a general kernel API fix. Select only recognized module versions/hashes,
back up the original addon, and decline unknown versions. VS Code updates may
replace the addon and require a new compatibility check.

### Implementation milestones

1. Extract and pin the installed native interface; validate a minimal Node-API
   addon in the actual Electron pty host.
2. Bridge WinPTY and verify cmd input/output, resize and exit inside an isolated
   VS Code profile. Keep the user's working installation recoverable.
3. Validate Ctrl+C, repeated creation/closure, no orphan processes, Japanese text,
   PowerShell/Git Bash and full-screen redraw.
4. Validate normal shell tasks, debugger integratedTerminal requests, working
   directories, environment, and VxKex propagation to shell children.
5. Test both Vista and Server 2008, then package with explicit version matching.

The extension alternative uses the documented
[Pseudoterminal API](https://code.visualstudio.com/api/references/vscode-api#Pseudoterminal)
for writes, input, resize and close. It can offer a usable custom terminal sooner,
but should not be described as restoring every built-in terminal integration.

## Implementation and PowerShell follow-up (2026-09-30)

The WinPTY-backed Node-API adapter in `tools/VistaPty` is installed in the Server
2008 VM's VS Code 1.139.1. The integrated cmd terminal, shell tasks, resizing,
parallel sessions, Ctrl+C, Japanese input/output, and exit status were verified.
This supersedes the earlier feasibility-only status above.

PowerShell 3 initially exited `-65536`. Its output was `CLR の開始が HRESULT
80004005 で失敗しました。`. Direct guest execution worked, but both WinPTY and plain
child-process launches from the VxKex-enabled VS Code extension host failed.
This identified inherited VxKex propagation as the trigger. `KexDll/propagte.c`
now leaves the system PowerShell child to start normally; a separately configured
PowerShell IFEO entry still applies. Both 32-bit and 64-bit KexDll builds were
deployed to Server 2008 and copied to the Installer directory.

With VS Code shell integration disabled, the default integrated PowerShell
terminal created an output marker and exited with the requested code 31. With
shell integration enabled, its command did not complete in the same test.
The VM's normal user settings now set
`terminal.integrated.shellIntegration.enabled=false` and retain
`terminal.integrated.windowsUseConptyDll=true`. The normal editor was restarted
gracefully after deployment. The isolated extension-host test confirms the
terminal behavior; a manual Ctrl+@ check in the restored user window remains
useful. Vista itself has not been retested.
