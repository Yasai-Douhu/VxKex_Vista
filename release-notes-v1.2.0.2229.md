# VxKex Vista 1.2.0.2229

Compatibility milestone for Windows Vista and Windows Server 2008.

This release fixes the VS Code 1.139.1 User Installer launch path and Git HTTPS console clone on NT 6.0:

- handles the Inno Setup AVX dispatch used by the installer and extracted body;
- redirects the delay-loaded USER32 DPI API to the Vista compatibility layer;
- supplies the Windows 7 per-user Programs known-folder paths on Vista;
- routes dynamically resolved `SHGetKnownFolderPath` calls through KxUser;
- preserves WOW64 child-process version spoofing and shell-property configuration;
- rejects legacy CSRSS console handles in `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` on NT 6.0, allowing Git to safely fall back to standard handle inheritance and fixing `fatal: remote helper 'https' aborted session` when stderr is attached to a console.

Validation was performed on Windows Server 2008 x64. The VS Code installer reaches its
license wizard with responsive controls. Git HTTPS clone and `git fsck --full` succeed
in console environments.
