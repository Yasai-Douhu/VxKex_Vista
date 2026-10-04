param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(Test-Path $OutputDirectory){throw 'Preserve previous fixture builds'}
New-Item -ItemType Directory $OutputDirectory|Out-Null
$directory=(Resolve-Path $OutputDirectory).Path
$vc='C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC'
$sdk='C:\Program Files\Microsoft SDKs\Windows\v7.1'
[IO.File]::WriteAllText("$directory\imports.def","LIBRARY ntdll.dll`r`nEXPORTS`r`n NtQuerySystemInformationEx`r`n",[Text.ASCIIEncoding]::new())
# Only reference the imported address. Do not call an API or change the guest.
[IO.File]::WriteAllText("$directory\fixture.c",'__declspec(dllimport) void NtQuerySystemInformationEx(void); int main(void) { void (* volatile address)(void) = NtQuerySystemInformationEx; return address ? 0 : 1; }',[Text.ASCIIEncoding]::new())
$originalPath=$env:PATH
foreach($arch in @('x86','x64')){
 $suffix=if($arch -eq 'x64'){'\amd64'}else{''};$sdkSuffix=if($arch -eq 'x64'){'\x64'}else{''}
 $env:PATH="$vc\bin$suffix;$vc\bin;$(Split-Path $vc)\Common7\IDE;$originalPath"
 $env:INCLUDE="$sdk\Include;$vc\include";$env:LIB="$sdk\Lib$sdkSuffix;$vc\lib$suffix"
 & lib.exe /nologo "/def:$directory\imports.def" "/machine:$arch" "/out:$directory\imports-$arch.lib"
 if($LASTEXITCODE){throw 'Audit fixture import library failed'}
 & cl.exe /nologo /MT /O1 /W4 "/Fo$directory\fixture-$arch.obj" "/Fe$directory\fixture-$arch.exe" "$directory\fixture.c" /link /SUBSYSTEM:CONSOLE,6.0 "$directory\imports-$arch.lib"
 if($LASTEXITCODE){throw 'Audit fixture compile failed'}
 & "$vc\bin\dumpbin.exe" /imports "$directory\fixture-$arch.exe" > "$directory\fixture-$arch-imports.txt"
 if($LASTEXITCODE){throw 'Audit fixture independent import check failed'}
}
