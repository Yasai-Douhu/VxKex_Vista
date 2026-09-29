# VS Code Insiders startup on Server 2008

## 2026-09-29 investigation

Target: `C:\Users\Administrator\AppData\Local\programs\Microsoft VS Code Insiders\Code - Insiders.exe`, product version `1.140.0-insider`.
Reported failure: exception `80000003`, executable offset `06f6a6a9`.

The executable has ProductName `Visual Studio Code - Insiders`. The launch profile previously recognized only `Visual Studio Code`, so Insiders missed the Electron launch options. Its IFEO VistaRun configuration was already present. A diagnostic launch with the options explicitly supplied reached workbench initialization.

`VistaIsVSCode` in `00-Common-Headers/VistaLaunch.h` now recognizes both product names. Recognition does not depend on a filename or version. VistaRun automatically supplies the existing profile options:

```
--no-sandbox --disable-gpu --disable-gpu-sandbox --disable-software-rasterizer --use-gl=disabled
```

## Build and deployment

Built with `tools/VistaRun/build.ps1 -OutputDirectory audit/insiders-launcher`.
Updated the Server 2008 VM's `C:\VxKex\VistaRun.exe` and `Installer/VistaRun.exe`.
Guest backup: `C:\VxKex\VistaRun.pre-insiders-20260929.exe`.

Build output, Installer copy, and downloaded guest copy share SHA-256:

```
BD39152E0CA2078AE8364697A02DD3DAF378B3542F83C11A2C95F10E583624FC
```

## Verification

After closing the isolated diagnostic instance, launched the target executable without arguments through VMware guest commands. The resulting main process (PID 4184) had the compatibility flags automatically added, used the ordinary Code - Insiders profile, and displayed a fully rendered Welcome window. `Responding` was True. No screen interaction was needed.

Local evidence (audit files are not release contents):

- `audit/insiders-normal.png`: screenshot of the normal launch.
- `audit/insiders-state.txt`: window title and responsiveness.
- `audit/insiders-normal-processes.txt`: actual process command lines.
- `audit/insiders-console.txt`: isolated diagnostic workbench initialization log.

This verifies startup and Welcome rendering, not every editor feature. The profile disables Electron sandboxing and GPU acceleration, as for the stable edition. The precise breakpoint instruction was not separately reverse engineered. This change has not been committed or published as a release.
