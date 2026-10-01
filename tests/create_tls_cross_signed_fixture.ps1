[CmdletBinding()]
param(
    [string]$OpenSsl = 'C:\Program Files\Git\usr\bin\openssl.exe',
    [string]$OutputDirectory = "$PSScriptRoot\..\audit\tls-cross-signed"
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$OutputDirectory = (Resolve-Path $OutputDirectory).Path
function Invoke-TestOpenSsl([string[]]$Arguments) {
    & $OpenSsl @Arguments
    if ($LASTEXITCODE) { throw "Cross-signed fixture generation failed: $Arguments" }
}
foreach ($name in @('A','B')) {
    Invoke-TestOpenSsl @('req','-x509','-newkey','rsa:2048','-nodes',
        '-keyout',"$OutputDirectory\root-$name-key.pem",'-out',"$OutputDirectory\root-$name.pem",
        '-days','30','-subj',"/CN=VxKex cross-sign root $name",
        '-addext','basicConstraints=critical,CA:TRUE',
        '-addext','keyUsage=critical,keyCertSign,cRLSign')
    Invoke-TestOpenSsl @('x509','-in',"$OutputDirectory\root-$name.pem",'-outform','DER',
        '-out',"$OutputDirectory\root-$name.cer")
}
Invoke-TestOpenSsl @('req','-new','-newkey','rsa:2048','-nodes',
    '-keyout',"$OutputDirectory\intermediate-key.pem",'-out',"$OutputDirectory\intermediate.csr",
    '-subj','/CN=VxKex cross-sign intermediate')
Set-Content "$OutputDirectory\intermediate.ext" -Encoding ascii -Value @(
    'basicConstraints=critical,CA:TRUE,pathlen:0',
    'keyUsage=critical,keyCertSign,cRLSign',
    'subjectKeyIdentifier=hash','authorityKeyIdentifier=keyid,issuer')
foreach ($name in @('A','B')) {
    Invoke-TestOpenSsl @('x509','-req','-in',"$OutputDirectory\intermediate.csr",
        '-CA',"$OutputDirectory\root-$name.pem",'-CAkey',"$OutputDirectory\root-$name-key.pem",
        '-set_serial','2','-days','14','-extfile',"$OutputDirectory\intermediate.ext",
        '-out',"$OutputDirectory\intermediate-$name.pem")
}
Invoke-TestOpenSsl @('req','-new','-newkey','rsa:2048','-nodes',
    '-keyout',"$OutputDirectory\server-key.pem",'-out',"$OutputDirectory\server.csr",
    '-subj','/CN=localhost')
Set-Content "$OutputDirectory\server.ext" -Encoding ascii -Value @(
    'basicConstraints=critical,CA:FALSE','keyUsage=critical,digitalSignature,keyEncipherment',
    'extendedKeyUsage=serverAuth','subjectAltName=DNS:localhost',
    'authorityKeyIdentifier=keyid')
Invoke-TestOpenSsl @('x509','-req','-in',"$OutputDirectory\server.csr",
    '-CA',"$OutputDirectory\intermediate-A.pem",'-CAkey',"$OutputDirectory\intermediate-key.pem",
    '-set_serial','3','-days','7','-extfile',"$OutputDirectory\server.ext",
    '-out',"$OutputDirectory\server.pem")
$chain = @('server.pem','intermediate-A.pem','intermediate-B.pem') |
    ForEach-Object { Get-Content -Raw -LiteralPath "$OutputDirectory\$_" }
[IO.File]::WriteAllText("$OutputDirectory\server-chain.pem", ($chain -join "`n"), [Text.Encoding]::ASCII)
$certificates = [Security.Cryptography.X509Certificates.X509Certificate2Collection]::new()
[void]$certificates.Add([Security.Cryptography.X509Certificates.X509Certificate2]::new("$OutputDirectory\root-B.cer"))
[IO.File]::WriteAllBytes("$OutputDirectory\ROOT.sst",
    $certificates.Export([Security.Cryptography.X509Certificates.X509ContentType]::SerializedStore))
Write-Output "Fixture: temporarily trust root A in the test Windows machine store; trust only root B in ROOT.sst. Remove root A after testing."
