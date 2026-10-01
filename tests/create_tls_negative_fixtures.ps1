[CmdletBinding()]
param(
    [string]$OpenSsl = 'C:\Program Files\Git\usr\bin\openssl.exe',
    [string]$FixtureDirectory = "$PSScriptRoot\..\audit\tls-fixture"
)
$ErrorActionPreference = 'Stop'
$FixtureDirectory = (Resolve-Path $FixtureDirectory).Path
function Invoke-TestOpenSsl([string[]]$Arguments) {
    & $OpenSsl @Arguments
    if ($LASTEXITCODE) { throw "OpenSSL fixture generation failed: $Arguments" }
}
$negativeDb = Join-Path $FixtureDirectory 'negative-ca-db'
New-Item -ItemType Directory -Force -Path $negativeDb | Out-Null
Set-Content -LiteralPath "$negativeDb\index.txt" -Encoding ascii -Value '' -NoNewline
Set-Content -LiteralPath "$negativeDb\serial" -Encoding ascii -Value '02'
$sslDirectory = $FixtureDirectory.Replace('\','/')
Set-Content -LiteralPath "$negativeDb\ca.cnf" -Encoding ascii -Value @(
    '[ca]', 'default_ca=fixture', '[fixture]',
    "database=$sslDirectory/negative-ca-db/index.txt",
    "serial=$sslDirectory/negative-ca-db/serial",
    "new_certs_dir=$sslDirectory/negative-ca-db",
    "private_key=$sslDirectory/root-key.pem",
    "certificate=$sslDirectory/root.pem",
    'default_md=sha256', 'policy=fixture_policy',
    '[fixture_policy]', 'commonName=supplied')
$testStart = [DateTime]::UtcNow.AddDays(-3).ToString("yyMMddHHmmss'Z'")
$testEnd = [DateTime]::UtcNow.AddDays(-1).ToString("yyMMddHHmmss'Z'")
Invoke-TestOpenSsl @('ca','-batch','-notext','-config',"$negativeDb\ca.cnf",
    '-in',"$FixtureDirectory\server.csr",'-startdate',$testStart,'-enddate',$testEnd,
    '-extfile',"$FixtureDirectory\server.ext",'-out',"$FixtureDirectory\expired.pem")
$clientOnlyExtensions = (Get-Content -LiteralPath "$FixtureDirectory\server.ext") -replace
    'extendedKeyUsage=serverAuth','extendedKeyUsage=clientAuth'
Set-Content -LiteralPath "$FixtureDirectory\client-only.ext" -Encoding ascii -Value $clientOnlyExtensions
Invoke-TestOpenSsl @('x509','-req','-in',"$FixtureDirectory\server.csr",
    '-CA',"$FixtureDirectory\root.pem",'-CAkey',"$FixtureDirectory\root-key.pem",
    '-set_serial','3','-days','7','-extfile',"$FixtureDirectory\client-only.ext",
    '-out',"$FixtureDirectory\client-only.pem")
Invoke-TestOpenSsl @('x509','-in',"$FixtureDirectory\server.pem",'-outform','DER',
    '-out',"$FixtureDirectory\server.cer")
$encodedCertificate = [IO.File]::ReadAllBytes("$FixtureDirectory\server.cer")
# Change only the signature BIT STRING, retaining the server public key and DER layout.
$encodedCertificate[$encodedCertificate.Length - 1] = $encodedCertificate[$encodedCertificate.Length - 1] -bxor 1
[IO.File]::WriteAllBytes("$FixtureDirectory\bad-signature.cer",$encodedCertificate)
Invoke-TestOpenSsl @('x509','-inform','DER','-in',"$FixtureDirectory\bad-signature.cer",
    '-out',"$FixtureDirectory\bad-signature.pem")
Write-Output "Negative TLS fixtures saved: $FixtureDirectory"
