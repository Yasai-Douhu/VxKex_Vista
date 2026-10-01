# High-priority distribution integration (2026-10-01)

Branch: `codex/next-parity-high`.

`tools/Stage-NextParityHigh.ps1` stages the preserved Installer contents, builds
both KexDll/KxAdvapi/KxCryp/KxSChanl, adds the unchanged NEXT ROOT.sst, builds the
current native/WOW64 configuration helper archives, both settings GUIs and both
viewers, and builds the native transactional setup. build_vistasetup.ps1 now
accepts OutputDirectory to avoid overwriting earlier diagnostic binaries.

```powershell
powershell -ExecutionPolicy Bypass -File tools/Stage-NextParityHigh.ps1 -PackageDirectory audit/NextParityHighPackage -BuildDirectory audit/build-next-high-package-v2
```

The created candidate has 58 files and 13 source-built binary replacements.
`audit/build-next-high-package-v2/high-package-manifest.json` records file hashes,
replacement provenance and relevant source hashes. New build/package directories
are required; no previous candidate is overwritten. SHA256 uses .NET directly
because the initial orchestration could not autoload Get-FileHash in its child
environment. That initial build never reached package creation; its logs remain
under audit/build-next-high-package. The corrected orchestration completed.

The clone received every candidate file. The guarded deployment probe's
`--integrated` mode invokes that candidate's VistaSetup and KexCfg, verifies the
physical system DLLs, root bundle and all five installed frontend images against
the candidate, exercises native/applied/native registration in x64 and WOW64,
and removes only its owned profiles and installation. Machine SSP configuration
is unchanged throughout. `audit/high-package-deployment.txt` has Failures=0.

After this passed, the original Installer tree was preserved in
`audit/Installer-before-next-high`. Only the 13 declared replacements and
Certificates/ROOT.sst were copied into Installer. Unrelated candidate assets
were checked against the existing Installer before replacement. All 58 final
Installer files match the candidate hashes; no mismatch was found. The record
is `audit/next-high-installer-integration.json`. VistaPty and other baseline
compatibility components are preserved. The original user VM was not modified.

Distribution integration is now performed. The phase completion audit and final
four-area status report remain required; this integration record alone does not
claim completion. No git commit or release publication was performed.
