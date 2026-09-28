; Build with Inno Setup 6.0.5. No software payload, uninstaller or uninstall key.
; Installs only a diagnostic shortcut under C:\VxKexProbe\ShellLinkInstaller.
[Setup]
AppName=VxKex ShellLink Probe
AppVersion=1.0
DefaultDirName=C:\VxKexProbe\ShellLinkInstaller
DisableDirPage=yes
DisableProgramGroupPage=yes
Uninstallable=no
CreateUninstallRegKey=no
PrivilegesRequired=lowest
OutputDir=..\audit\shelllink-installer
OutputBaseFilename=inno-shelllink-setup
Compression=none

[Dirs]
Name: "{app}"

[Icons]
Name: "{app}\ShellLink"; Filename: "{sys}\notepad.exe"
