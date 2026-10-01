# NEXT parity TLS package staging

`tools/Stage-NextParityTls.ps1` creates a new diagnostic package from the current
Installer tree. It builds both architectures of KexDll, KxAdvapi, KxCryp and
KxSChanl from the current sources and replaces only those eight files in the
new package. It adds the unmodified NEXT ROOT.sst under Certificates.

```powershell
powershell -ExecutionPolicy Bypass -File tools/Stage-NextParityTls.ps1 `
  -PackageDirectory audit/NextParityTlsPackage `
  -BuildDirectory audit/build-next-tls-package
```

An existing package directory is rejected, rather than merged or deleted. Use a
new path for another candidate. Output DLLs are checked for PE architecture and
DLL characteristics before staging. The build directory contains per-phase
stdout/stderr logs and package-manifest.json with all file hashes and explicit
replacement sources. This manifest is outside the installed package.

The x64 extended build uses NoStage to avoid replacing VistaDLLs. Both extended
builds prefer the matching newly built KxCryp import library. The x86 build now
accepts OutputRoot. KexDll builds use their architecture's explicit KexPathCch
library. No Installer assets or installed VM files are changed by this command.

Build processes write stdout/stderr directly to OS files. The launcher waits for
the actual build process, not its long-lived MSPDB compiler-server descendants.
Earlier pipeline orchestration blocked a compiler diagnostic write; a normal
minidump captured its NtWriteFile/WriteFile/msvcr100 write stack. Only the owned
build processes were terminated after confirming their parent chain. A later
completed child build exposed Start-Process -Wait waiting for MSPDB descendants;
that orchestration was replaced by Process.WaitForExit. Shared compiler servers
were not stopped. These were host build-orchestration failures, not VM TLS faults.

## Current candidate

The first complete stage contains 57 files. ROOT.sst is byte-identical to NEXT:
SHA-256 3F39EA1770E162443CDE855B28760FBDC7F7107E3FC9EB7BFAFA61CFD335AFD2.
Its role is the explicit root bundle already exercised by earlier TLS probes;
the stage does not import certificates into Windows' global Root store.

The stage is **not a verified release**. The Installer frontend remains the
existing baseline in this candidate; the newer development GUI has separate
verification artifacts. TLS runtime deployment, registered/native process
separation, all dependency loading, TLS failure-path tests and the remaining
high-priority parity work still have to pass before final distribution.

Registration probes now require the actual KxSChanl/KexDll/KxAdvapi modules,
successful credential acquisition and release, and preserved CredSSP. Native
mode requires the provider and compatibility modules to remain absent:

```powershell
powershell -ExecutionPolicy Bypass -File tests/build_kxschanl_registration_probe.ps1 -Architecture x64
powershell -ExecutionPolicy Bypass -File tests/build_kxschanl_registration_probe.ps1 -Architecture x86
```

Run with `--native` before applying VxKex, then without arguments in an applied
test process. Their native OS imports are intentional: this checks the actual
process-local registration path. Merely returning SEC_E_OK from package
enumeration no longer counts as registration success. These strengthened probes
have built and passed against this staged package in the disposable Server 2008 VM.

## Deployment and registration verification (2026-10-01)

`tests/build_tls_package_deployment_probe.ps1` builds the native x64 guarded
deployment probe. It requires the exact disposable marker, an elevated operator,
no installation/preferences/cleanup handler, and no owned image fixtures before
performing any installation. It installs the actual 57-file candidate through
VistaSetup with the caller's captured SID. All eight rebuilt DLLs in physical
System32/SysWOW64 and the installed ROOT.sst match the package byte-for-byte.

Both architectures pass native-before, applied, and native-after registration
checks. Applied processes enumerate nine packages including KxSChanl, load
KexDll/KxAdvapi/KxSChanl, and acquire/release outbound credentials successfully.
Native processes enumerate eight packages with no compatibility modules.
CredSSP remains present throughout. The machine SecurityProviders value is
opened read-only and remains byte-identical, including its registry type, after
the tests and teardown. No global provider registration is used.

Evidence: `audit/tls-package-deployment.txt` has Failures=0; the six
`audit/tls-{native-before,registered,native-after}-{x64,x86}.txt` reports each
have Passed=1. The probe deletes only its created profiles/images, uninstalls
the diagnostic deployment, and verifies preferences/installation/cleanup
handler are absent. An initial run passed runtime checks but incorrectly
counted an already-absent cleanup handler as failure; its evidence is retained
in `audit/tls-package-deployment-first.txt`. The corrected check requires actual
absence after teardown, and the complete rerun passed.

This proves deployment and process-local registration, not a complete TLS
handshake or the remaining revocation/fragmentation/negative-path coverage.
Installer distribution assets and the original user VM remain unchanged.
