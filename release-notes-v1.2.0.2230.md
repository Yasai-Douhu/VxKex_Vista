# VxKex Vista 1.2.0.2230

Compatibility milestone for Windows Vista and Windows Server 2008.

### Key Changes in this Release:

- **Obsidian & Electron Application Support**:
  - Implemented `ChangeWindowMessageFilterEx` in `KxUser`, safely mapping to process-wide UIPI message filters on NT 6.0.
  - Extended `VistaLaunch` and `VistaRun` with Electron launch profile detection to support Obsidian alongside VS Code.

- **Inno Setup Shortcut Creation**:
  - Fixed `IShellLink` COM interface interception on Vista for modern Inno Setup installers, resolving shortcut generation issues.

- **Sublime Text 4 & Python 3.14 Networking**:
  - Implemented missing timer APIs (`QueryInterruptTime` / unbiased time) in `KxBase`.
  - Added `WSARecvMsg` compatibility shims in `KxNet` for Python 3.14 socket operations used by Package Control.

- **Installer Enhancement**:
  - Added `[2] Update VxKex` option in `install.bat` to allow seamless in-place updating of existing VxKex installations while preserving custom application compatibility configurations.
