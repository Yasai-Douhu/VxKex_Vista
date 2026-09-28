# Sublime Text 4213 / Python 3.14 on NT 6.0

## 2026-09-28: Package Control (WinError 10041)

Reproduced in Server 2008 x64, using Sublime's actual Python 3.14 host.
CPython calls WSASocketW with OVERLAPPED | NO_HANDLE_INHERIT (0x81).
Vista does not implement the latter flag. Native calls with protocol 0 return
WSAEPROTOTYPE (10041); explicit TCP/UDP protocols return 10022. The failure
occurs before TLS. Disabling certificate validation would not fix it.

KxNet now implements WSASocketA/W rather than forwarding them unconditionally.
On real NT 6.0 only, it removes 0x80, creates the socket, and clears
HANDLE_FLAG_INHERIT using SetHandleInformation. Other arguments and flags are
preserved. Failed socket creation preserves the Winsock error; a failure to
clear inheritance closes the socket and returns failure. Other OS versions
retain the native call. Export ordinals 99 and 100 are preserved.

The Vista fallback creates the socket and clears inheritance in separate
operations; it cannot provide the atomic creation guarantee of newer Winsock.

### Verification

Both x64 and x86 KxNet builds succeeded with VS2010 and SDK 7.1:

```powershell
powershell.exe -File build_kxnet.ps1 -Architecture x64 -OutputDirectory audit/plugin314-net64
powershell.exe -File build_kxnet.ps1 -Architecture x86 -OutputDirectory audit/plugin314-net32
```

Runtime verification used a portable copy at `C:\VxKexProbe\Sublime4213`,
with a separate Data profile; the normal user profile was not modified.
`tests/sublime_package_control_probe.py` must be placed in a package configured
with `.python-version` containing `3.14`. It uses explicit checks because
Sublime's optimized Python can omit assertions.

- IPv4 TCP (protocol 0 and 6), IPv6 TCP, and UDP succeed and are non-inheritable.
- ANSI and Unicode wrappers with 0x81 return non-inheritable sockets.
- Calls with OVERLAPPED alone retain native behavior (inheritable in this VM).
- An invalid TCP socket/UDP protocol combination still fails.
- Verified HTTPS download of Package Control and its detached signature succeeds.
- Sublime's shipped signature verifier accepts the archive; it is installed in
  the isolated profile, and Package Control modules load.
- Archive: 502007 bytes, SHA256
  `8c7972e7d66630193f525adf4b8cd92cde4b6f79cff588367cde545275fd6852`.

Evidence: `audit/packagecontrol-result-verified.json` and
`audit/plugin314-network-after.json`. These are local diagnostic artifacts.
This verifies installation, not every Package Control repository or operation.
Runtime socket checks were x64; x86 was built and deployed, not exercised by
Sublime's x64 host.

### Deployment

Updated Installer/KxNet.dll and Installer/Kex32/KxNet.dll and VM copies in
System32, SysWOW64, C:\VxKex and C:\VxKex\Kex32. Previous VM copies have suffix
`.pre-packagecontrol-20260928`. System DLLs retrieved from the VM match the
packaged hashes:

| Architecture | SHA256 |
| --- | --- |
| x64 | 58B58AA4B085609EFA7320E3D0ACA5C7E8EF5814B946E9A91BFDD24DDF94EAC7 |
| x86 | D3A5C8DDE57F3478273FE18C517C15F5984E3FC5F9947E47161A86581F070D8E |

Fully exit and restart the normal Sublime instance before retrying Install
Package Control; an already-running plugin host retains its old DLL.

## Earlier in this investigation: plugin_host-3.14 startup crash

The initial ntdll access violation occurred during loading: KxBase forwarded
SetWaitableTimerEx to a kernel32 export absent on Vista. The observed loader
failure subsequently referenced a forwarder string in the unloaded KxBase.
KxBase now exports Ext_SetWaitableTimerEx, delegating to the native API when
available and using SetWaitableTimer otherwise. The fallback preserves timer
and APC arguments; wake-reason metadata and coalescing are unavailable on Vista.

Updated x64/x86 KxBase were deployed with `.pre-plugin314-20260928` backups.
The real isolated plugin host loads Python 3.14.6, imports SSL and SQLite, and
runs sleep/threading timer checks. WOW64 child propagation and Inno shortcut
creation regression checks also passed after this change. See
`tests/sublime_python314_probe.py` and local `audit/plugin314-*.json` logs.

## References

- [Microsoft WSASocketW: flag availability](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsasocketw)
- [CPython 3.14 socket implementation](https://github.com/python/cpython/blob/3.14/Modules/socketmodule.c)
- [Microsoft SetWaitableTimerEx](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimerex)

## Package Control's own downloads: WinINet error 12029

After installation, Package Control 4.2.7 selected its Windows WinINet backend.
On the same Server 2008 VM, its own WinINetDownloader failed with 12029 for
both the GitHub-hosted channel and the 4.2.8 update ZIP. Its UrlLibDownloader
successfully fetched the same URLs, with normal certificate and hostname
validation enabled:

- Channel: 271273 bytes, parsed schema_version 4.0.0.
- Update ZIP: 512891 bytes, ZIP signature confirmed.

This isolates the failure to the WinINet communication path; 12029 alone does
not establish a specific TLS protocol, cipher, proxy, or certificate cause.
The previous KxNet socket fix does not replace WinINet's TLS implementation.

Applied the supported Package Control setting to the normal Administrator
Sublime user profile, preserving other settings and other platform entries:

```json
{
    "downloader_precedence": {
        "windows": ["urllib"]
    }
}
```

Backup: `Package Control.sublime-settings.pre-vxkex-urllib-20260928` beside the
user settings file. The one-shot settings helper removes itself after saving.
An existing cached downloader may need a full Sublime restart to pick up the
change. No additional VxKex DLL change is required for this workaround.

Local evidence: `audit/pc-backends.json`, `audit/pc-user-fix.json`,
`audit/pc-user-verify.json`. These tests download content without installing or
upgrading additional packages.

Reference: [Package Control downloader_precedence](https://packagecontrol.io/docs/settings).

Normal-profile verification also passed: DownloadManager selected
UrlLibDownloader and retrieved/parsed the channel (271273 bytes, schema 4.0.0).
The failed upgrade had left Package Control in ignored_packages; removed only
that entry after backing up Preferences.sublime-settings with suffix
`.pre-vxkex-pc-enable-20260928`. Other ignored packages were preserved.
