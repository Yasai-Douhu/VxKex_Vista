param(
 [Parameter(Mandatory=$true)][string]$NativeReceipt,
 [Parameter(Mandatory=$true)][string]$LinkReceipt,
 [Parameter(Mandatory=$true)][string]$AdapterReceipt,
 [Parameter(Mandatory=$true)][string]$IfeoReceipt,
 [Parameter(Mandatory=$true)][string]$Output
)
$ErrorActionPreference='Stop'
if(Test-Path $Output){throw 'Select a fresh analysis output'}
$native=Get-Content $NativeReceipt -Raw|ConvertFrom-Json
$link=Get-Content $LinkReceipt -Raw|ConvertFrom-Json
$adapter=Get-Content $AdapterReceipt -Raw|ConvertFrom-Json
$ifeo=Get-Content $IfeoReceipt -Raw|ConvertFrom-Json
if($native.State -ne 'Passed' -or $native.Adapter -or $native.Results.Count -ne 2 -or $link.State -ne 'Passed' -or !$link.Adapter -or $link.Results.Count -ne 2 -or $adapter.State -ne 'Passed' -or $adapter.Results.Count -ne 2 -or $ifeo.State -ne 'Passed' -or $ifeo.DriverExit -ne 0 -or $ifeo.Results.Count -ne 2){throw 'Require both native and candidate link matrices, scratch boundary comparisons and ordinary IFEO'}
function Matrix([string]$text){
 $matches=@([regex]::Matches($text,'(?m)^Mode=NtOpenKeyEx-option Attributes=(00000040|00000140) Options=(00000000|00000008) Status=00000000\r?\nCheck=query-opened-name Pass=1\r?\nName=([^\r\n]+) Expected=MATCH\r?\nCheck=open-link-semantics Pass=1'))
 if($matches.Count -ne 4){throw 'Missing actual native-option / object-attribute matrix'}
 $pairs=@()
 foreach($m in $matches){$kind=if($m.Groups[1].Value -eq '00000140'){'Link'}else{'Target'};if(!$m.Groups[3].Value.EndsWith("\$kind")){throw 'Unexpected opened object'};$pairs+="$($m.Groups[1].Value):$($m.Groups[2].Value)"}
 if(@($pairs|Select-Object -Unique).Count -ne 4){throw 'Repeated matrix input'}
}
function Cleanup([string]$text){
 foreach($step in @('delete-link-by-owned-handle','delete-target-by-owned-handle','delete-owned-base','owned-base-absent','no-handle-growth')){if($text -notmatch "Check=$step Pass=1"){throw 'Owned key cleanup/resource check missing'}}
 if($text -notmatch 'Failures=0 Result=PASS'){throw 'Link fixture failed'}
}
foreach($arch in @('x86','x64')){
 $n=@($native.Results|Where-Object Architecture -eq $arch);$l=@($link.Results|Where-Object Architecture -eq $arch)
 if($n.Count -ne 1 -or $l.Count -ne 1 -or !$n[0].Passed -or !$l[0].Passed){throw 'Missing architecture'}
 Matrix $n[0].HostOutput;Cleanup $n[0].HostOutput;Cleanup $n[0].GuestOutput
 if($n[0].GuestOutput -notmatch 'NativeEx=0 KexDllLoaded=0' -or [regex]::Matches($n[0].GuestOutput,'Check=open-link-semantics Pass=1').Count -ne 4){throw 'Missing NT6 legacy reference'}
 Matrix $l[0].HostOutput;Matrix $l[0].GuestOutput;Cleanup $l[0].HostOutput;Cleanup $l[0].GuestOutput
 $a=@($adapter.Results|Where-Object Architecture -eq $arch);$i=@($ifeo.Results|Where-Object Architecture -eq $arch)
 if($a.Count -ne 1 -or $i.Count -ne 1 -or !$i[0].Passed){throw 'Missing boundary architecture'}
 foreach($text in @($a[0].HostOutput,$a[0].Output,$i[0].Output)){
  if([regex]::Matches($text,'(?m)^Case=').Count -ne 280 -or [regex]::Matches($text,'(?m)^Case=.*Option=0 ').Count -ne 140 -or [regex]::Matches($text,'(?m)^Case=.*Option=8 ').Count -ne 140 -or $text -match 'Match=0' -or $text -notmatch 'Repeated=1000 HandleDelta=0' -or $text -notmatch 'Failures=0 Result=PASS'){throw 'Boundary/native/TLS comparison incomplete'}
 }
 foreach($text in @($a[0].Output,$i[0].Output)){if([regex]::Matches($text,'(?m)^Unsupported=.*Status=c00000bb OutputUntouched=1').Count -ne 7 -or $text -match '(?m)^Unsupported=00000008'){throw 'Extended-option scope incorrect'}}
 if($i[0].Output -notmatch 'StaticImportEqual=1 EarlyKexDllLoaded=1'){throw 'No ordinary static import proof'}
}
if($ifeo.DriverOutput -notmatch 'PASS fresh deployment state restored' -or [regex]::Matches($ifeo.DriverOutput,'PASS owned IFEO key absent after cleanup').Count -ne 2){throw 'IFEO deployment cleanup missing'}
$hashes=@{};foreach($path in @($NativeReceipt,$LinkReceipt,$AdapterReceipt,$IfeoReceipt)){$hashes[(Split-Path (Split-Path $path) -Leaf)]=(Get-FileHash $path).Hash}
[pscustomobject]@{ReceiptSHA256=$hashes;AnalyzerSHA256=(Get-FileHash $PSCommandPath).Hash;Architectures=@('x86','x64');NativeMatrixPreservesObjectAttributes=$true;BoundaryCasesPerArchitecture=280;UnsupportedOptionCount=7;OrdinaryIfeoPassed=$true;Limits='Owned volatile links and existing native comparison fixture on one modern host and NT6 Server clone. Native NtOpenKeyEx delegation observed on the host; NT6 uses unchanged NtOpenKey arguments for options0/8. Backup/restore, registry virtualization, arbitrary security descriptors/QOS, transaction roots, deletion races, native x86 OS and Vista IFEO remain unverified or unsupported.'}|ConvertTo-Json -Depth 6|Set-Content $Output -Encoding UTF8
