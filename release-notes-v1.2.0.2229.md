# VxKex Vista 1.2.0.2229

Compatibility milestone for Windows Vista and Windows Server 2008.

This release fixes the VS Code 1.139.1 User Installer launch path on NT 6.0:

- handles the Inno Setup AVX dispatch used by the installer and extracted body;
- redirects the delay-loaded USER32 DPI API to the Vista compatibility layer;
- supplies the Windows 7 per-user Programs known-folder paths on Vista;
- routes dynamically resolved `SHGetKnownFolderPath` calls through KxUser;
- preserves WOW64 child-process version spoofing and shell-property configuration.

Validation was performed on Windows Server 2008 x64. The installer reaches its
license wizard with responsive controls. No application installation is included
in the validation.
