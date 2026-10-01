param(
	[Parameter(Mandatory=$true)][string]$JdkRoot,
	[string]$ToolchainJdkRoot,
	[string]$VscodeSettingsPath = (Join-Path $env:APPDATA 'Code\User\settings.json')
)

$ErrorActionPreference = 'Stop'
$selector = '-Djava.nio.channels.spi.SelectorProvider=sun.nio.ch.WindowsSelectorProvider'

function Get-JdkMajor([string]$Root) {
	$releasePath = Join-Path $Root 'release'
	if (!(Test-Path -LiteralPath (Join-Path $Root 'bin\java.exe')) -or
		!(Test-Path -LiteralPath $releasePath)) {
		throw "Not a JDK installation: $Root"
	}
	$version = @(Get-Content -LiteralPath $releasePath | Where-Object {$_ -match '^JAVA_VERSION='})
	if ($version.Count -ne 1 -or $version[0] -notmatch '^JAVA_VERSION="?([0-9]+)') {
		throw "Cannot read the JDK version: $releasePath"
	}
	$major = [int]$Matches[1]
	if ($major -lt 17) { throw "Java 17 or newer is required: $Root" }
	return $major
}

$major = Get-JdkMajor $JdkRoot
$JdkRoot = (Get-Item -LiteralPath $JdkRoot).FullName
if ($ToolchainJdkRoot) {
	$toolchainMajor = Get-JdkMajor $ToolchainJdkRoot
	$ToolchainJdkRoot = (Get-Item -LiteralPath $ToolchainJdkRoot).FullName
}

$directory = Split-Path -Parent $VscodeSettingsPath
if (!(Test-Path -LiteralPath $directory)) {
	New-Item -ItemType Directory -Path $directory -Force | Out-Null
}
if (Test-Path -LiteralPath $VscodeSettingsPath) {
	$settings = Get-Content -LiteralPath $VscodeSettingsPath -Raw | ConvertFrom-Json
	$backup = $VscodeSettingsPath + '.pre-java-gradle'
	if (!(Test-Path -LiteralPath $backup)) {
		Copy-Item -LiteralPath $VscodeSettingsPath -Destination $backup
	}
} else {
	$settings = New-Object PSObject
}

$vmargs = $settings.'java.jdt.ls.vmargs'
if (!$vmargs) {
	$vmargs = '-XX:+UseParallelGC -XX:GCTimeRatio=4 -XX:AdaptiveSizePolicyWeight=90 -Dsun.zip.disableMemoryMapping=true -Xmx2G -Xms100m -Xlog:disable'
}
if ($vmargs -notlike "*$selector*") { $vmargs += " $selector" }
$settings | Add-Member -NotePropertyName 'java.jdt.ls.vmargs' -NotePropertyValue $vmargs -Force
$settings | Add-Member -NotePropertyName 'java.import.gradle.java.home' -NotePropertyValue $JdkRoot -Force

$gradleJvmArgs = $settings.'java.import.gradle.jvmArguments'
if (!$gradleJvmArgs) { $gradleJvmArgs = '' }
if ($gradleJvmArgs -notlike "*$selector*") { $gradleJvmArgs = ($gradleJvmArgs + ' ' + $selector).Trim() }
$settings | Add-Member -NotePropertyName 'java.import.gradle.jvmArguments' -NotePropertyValue $gradleJvmArgs -Force

$gradleArgs = $settings.'java.import.gradle.arguments'
if (!$gradleArgs) { $gradleArgs = '' }
if ($gradleArgs -notmatch '(^|\s)--no-watch-fs(\s|$)') {
	$gradleArgs = ($gradleArgs + ' --no-watch-fs').Trim()
}
$settings | Add-Member -NotePropertyName 'java.import.gradle.arguments' -NotePropertyValue $gradleArgs -Force
$settings | Add-Member -NotePropertyName 'java.gradle.buildServer.enabled' -NotePropertyValue 'off' -Force

if ($ToolchainJdkRoot) {
	$runtimes = @($settings.'java.configuration.runtimes' | Where-Object {$_})
	if (!@($runtimes | Where-Object {$_.path -eq $ToolchainJdkRoot}).Count) {
		$runtimes += [pscustomobject]@{
			name = "JavaSE-$toolchainMajor"
			path = $ToolchainJdkRoot
			default = !@($runtimes | Where-Object {$_.default}).Count
		}
	}
	if (!@($runtimes | Where-Object {$_.path -eq $JdkRoot}).Count) {
		$runtimes += [pscustomobject]@{name = "JavaSE-$major"; path = $JdkRoot}
	}
	$settings | Add-Member -NotePropertyName 'java.configuration.runtimes' -NotePropertyValue $runtimes -Force
}

$settings | ConvertTo-Json -Depth 20 | Out-File -LiteralPath $VscodeSettingsPath -Encoding UTF8

$current = [Environment]::GetEnvironmentVariable('JAVA_TOOL_OPTIONS', 'User')
if ($current -notlike "*$selector*") {
	$current = ($current + ' ' + $selector).Trim()
	[Environment]::SetEnvironmentVariable('JAVA_TOOL_OPTIONS', $current, 'User')
}

Write-Output "Configured Java $major for VS Code Gradle: $JdkRoot"
Write-Output "Updated VS Code settings: $VscodeSettingsPath"
Write-Output 'Restart VS Code from a new Windows session to inherit JAVA_TOOL_OPTIONS.'
