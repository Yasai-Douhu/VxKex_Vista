$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$vc = 'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC'
$sdk = 'C:\Program Files\Microsoft SDKs\Windows\v7.1'
$env:PATH = "$vc\bin\amd64;$vc\bin;$(Split-Path $vc)\Common7\IDE;$env:PATH"
$env:INCLUDE = "$vc\include;$sdk\Include"
$env:LIB = "$vc\lib\amd64;$sdk\Lib\x64"
# Native x64 is required to compare physical System32 and SysWOW64.
& cl.exe /nologo /MT /O1 /D_WIN32_WINNT=0x0600 "/Fe$root\audit\tls_package_deployment_probe.exe" "/Fo$root\audit\tls_package_deployment_probe.obj" "$PSScriptRoot\tls_package_deployment_probe.c" /link /SUBSYSTEM:CONSOLE,6.0 advapi32.lib shell32.lib
if ($LASTEXITCODE) { throw 'TLS deployment probe build failed' }
