# VistaPty: VS Code's node-pty interface backed by WinPTY

This optional application companion restores local terminals on NT 6.0 without
implementing the ConDrv kernel protocol. It replaces the native `conpty.node`
module, preserving the installed node-pty JavaScript and VS Code terminal UI.

## Verified target

- Server 2008 x64, VS Code 1.139.1, Electron 43.6.0 / Node 24.20.0 / Node-API 10.
- node-pty 1.2.0-beta.15, original conpty.node SHA-256:
  `3E81F5A9868A6F41316AB2895ED965554B4DCB733DF1C06EB06DEA85100EAB98`.
- WinPTY binaries from the previously extracted VS Code 1.70.2 distribution.
  Their hashes and initial native tests are in
  `docs/vscode-terminal-vista-research.md`.

Install refuses an unknown native module or a different VS Code version. This
does not assert compatibility with arbitrary node-pty versions or with Insiders.

## Build

Run `powershell -ExecutionPolicy Bypass -File tools\VistaPty\build.ps1` from the
repository. The build uses VS2010 x64 and SDK 7.1 and writes `audit/VistaPty`.
The static CRT avoids introducing a new runtime requirement. Node-API symbols
are resolved from the host executable; no version-specific Electron import
library or V8 ABI is used. The addon uses Node-API 8.

The vendored Node-API headers are from nodejs/node v22.0.0; the WinPTY API headers
are from rprichard/winpty. Their license files are under `include`. VS2010 C++
needs the small `compat/stdbool.h` shim. Do not remove upstream license files.

Place the tested winpty.dll and winpty-agent.exe beside conpty.node and
vista-pty-clear.exe. Packaged copies are under `Installer/VistaPty`.

## Install and restore

Close VS Code first. You can run the batch wrapper directly (which automatically bypasses execution policy restrictions) or the PowerShell script:

```cmd
.\install.cmd
```
*(If `-AppDirectory` is omitted, the installer automatically detects the VS Code / VS Code Insiders `resources\app` directory).*

Or specify the directory explicitly:

```cmd
.\install.cmd "C:\Users\Vista\AppData\Local\Programs\Microsoft VS Code\resources\app"
```

To restore the original node-pty module:
```cmd
.\uninstall.cmd
```

The script backs up conpty.node as `conpty.node.pre-vistapty` and adds three
adjacent support binaries. `terminal.integrated.windowsUseConptyDll` should stay
true. No general VxKex DLL, ASAR JavaScript or user's settings file is replaced.

On Vista/Server 2008, add these user settings for Windows PowerShell 3:

```json
{
  "terminal.integrated.windowsUseConptyDll": true,
  "terminal.integrated.shellIntegration.enabled": false
}
```

The VxKex KexDll build must also include the PowerShell child-process exception
in `KexDll/propagte.c`. Without it, the parent VS Code process propagates VxKex
into the system `powershell.exe`, and the CLR fails to start with HRESULT
80004005. An explicit IFEO configuration for PowerShell is still honored.

To restore, close VS Code and run `uninstall.ps1` with the same AppDirectory.
It verifies the backup and installed file hashes before restoring the original.
The original backup is retained; inspect it before a later reinstall (the
installer intentionally refuses an existing backup).

## Lifecycle

Each start creates a separate WinPTY agent and exposes its UTF-8/VT input and
output pipe names. Connect spawns the requested shell with its command line,
environment and working directory. A dedicated native waiter delivers process
exit through a Node-API thread-safe callback, avoiding the libuv thread-pool
limit. Auto-shutdown drains output and then exits the agent; teardown closes the
console and its attached children. Environment cleanup also stops outstanding
sessions. Clear runs a short-lived native helper attached to that console so
the Node host never has to change its own console attachment.

No-op success stubs are not used for process creation or console attachment.
WinPTY remains an older terminal backend: advanced ConPTY-only protocol features,
perfect redraw compatibility and every fullscreen application are not promised.

## Validation performed on 2026-09-30

The `validation` extension was launched in a separate development-host profile.
It is a test harness only and is **not required** for normal terminal use.

- Actual VS Code integrated terminal displayed cmd output and executed an echo
  command which created a marker file; screenshot captured.
- Its exit event reported code 7, matching `exit /b 7`.
- VS Code ShellExecution task created its marker and exited 0.
- Addon tests inside Electron passed six simultaneous sessions, resize, Japanese
  input/output, Ctrl+C interrupting ping, clear, normal exit and forced close.
- In Server 2008, Windows PowerShell 3 failed with exit `-65536` and CLR HRESULT
  `80004005` when inherited VxKex was active. It succeeded after excluding
  `powershell.exe` from automatic child propagation: native WinPTY exit 23,
  ordinary child-process invocation, and VS Code terminal command/exit 29.
- With shell integration enabled, VS Code's default PowerShell terminal did not
  complete a sent command. Disabling shell integration made its command run and
  exit with code 31. This does not affect basic terminal input/output.
- The normal editor was closed gracefully and reopened after updating KexDll;
  an unsaved `test.py` was restored by VS Code's hot-exit behavior.
- Deployment and package addon SHA-256 match:
  `ACE86F0F580E2E7D853A13A110FE5D43B050F40143AB283B7B40E28A8FE5A210`.

Evidence: `audit/VistaPty/extension-test-default-nointegration.txt`,
`extension-test-default-shell.txt`, `host-smoke.txt`, `integrated-terminal.png`.
Git Bash, debugger terminal requests, reconnect/persistence, IME interaction
and Vista VM behavior are not yet verified.

No Git commit or release publication was made in this implementation task.
