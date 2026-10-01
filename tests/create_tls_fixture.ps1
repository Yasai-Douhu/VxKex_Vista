[CmdletBinding()]
param(
    [string]$OpenSsl = 'C:\Program Files\Git\usr\bin\openssl.exe',
    [string]$OutputDirectory = "$PSScriptRoot\..\audit\tls-fixture"
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$OutputDirectory = (Resolve-Path $OutputDirectory).Path
function Invoke-TestOpenSsl([string[]]$Arguments) {
    & $OpenSsl @Arguments
    if ($LASTEXITCODE) { throw "OpenSSL fixture generation failed: $Arguments" }
}
Invoke-TestOpenSsl @('req','-x509','-newkey','rsa:2048','-nodes',
    '-keyout',"$OutputDirectory\root-key.pem",'-out',"$OutputDirectory\root.pem",
    '-days','30','-subj','/CN=VxKex local test root',
    '-addext','basicConstraints=critical,CA:TRUE',
    '-addext','keyUsage=critical,keyCertSign,cRLSign')
Invoke-TestOpenSsl @('req','-new','-newkey','rsa:2048','-nodes',
    '-keyout',"$OutputDirectory\server-key.pem",'-out',"$OutputDirectory\server.csr",
    '-subj','/CN=localhost')
Set-Content -LiteralPath "$OutputDirectory\server.ext" -Encoding ascii -Value @(
    'basicConstraints=critical,CA:FALSE',
    'keyUsage=critical,digitalSignature,keyEncipherment',
    'extendedKeyUsage=serverAuth',
    'subjectAltName=DNS:localhost')
Invoke-TestOpenSsl @('x509','-req','-in',"$OutputDirectory\server.csr",
    '-CA',"$OutputDirectory\root.pem",'-CAkey',"$OutputDirectory\root-key.pem",
    '-set_serial','1','-days','7','-extfile',"$OutputDirectory\server.ext",
    '-out',"$OutputDirectory\server.pem")
Invoke-TestOpenSsl @('x509','-in',"$OutputDirectory\root.pem",'-outform','DER',
    '-out',"$OutputDirectory\root.cer")
$testCertificate = [Security.Cryptography.X509Certificates.X509Certificate2]::new("$OutputDirectory\root.cer")
$testCertificates = [Security.Cryptography.X509Certificates.X509Certificate2Collection]::new()
[void]$testCertificates.Add($testCertificate)
[IO.File]::WriteAllBytes("$OutputDirectory\ROOT.sst",
    $testCertificates.Export([Security.Cryptography.X509Certificates.X509ContentType]::SerializedStore))
Write-Output "TLS fixture saved: $OutputDirectory"
