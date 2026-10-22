# GitStatus Windows packaging helper.
# Requires an installed Unreal Engine and the matching Windows C++ toolchain.
# Builds in a new timestamped folder; never deletes source or an existing build.
[CmdletBinding()]
param(
    [string]$EngineRoot,
    [string]$EngineVersion
)

$ErrorActionPreference = 'Stop'
$pluginRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $pluginRoot '..\..')).Path
$pluginFile = Join-Path $pluginRoot 'GitStatus.uplugin'

if (-not (Test-Path -LiteralPath $pluginFile -PathType Leaf)) {
    throw "Missing GitStatus.uplugin in $pluginRoot"
}

# When installed in a game project, use the .uproject EngineAssociation.
# When run from the standalone plugin checkout, default to team engine 5.7.
if ([string]::IsNullOrWhiteSpace($EngineVersion)) {
    $projectFile = Get-ChildItem -LiteralPath $projectRoot -Filter '*.uproject' -File -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($projectFile) {
        $association = (Get-Content -LiteralPath $projectFile.FullName -Raw | ConvertFrom-Json).EngineAssociation
        if ($association -match '^(5\.\d+)') {
            $EngineVersion = $Matches[1]
        }
    }
    if ([string]::IsNullOrWhiteSpace($EngineVersion)) {
        $EngineVersion = '5.7'
    }
}

if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    $EngineRoot = Join-Path 'C:\Program Files\Epic Games' ('UE_' + $EngineVersion)
}
$runUAT = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'

if (-not (Test-Path -LiteralPath $runUAT -PathType Leaf)) {
    throw "Cannot find RunUAT.bat at $runUAT. Run with -EngineRoot 'D:\Path\To\UE_$EngineVersion' if Unreal is installed elsewhere."
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmssfff'
$outputFolder = Join-Path $pluginRoot ('BuildOutput\GitStatus-UE' + $EngineVersion + '-Win64-' + $stamp)
$packageFolder = Join-Path $outputFolder 'GitStatus'
$archivePath = Join-Path $outputFolder ('GitStatus-UE' + $EngineVersion + '-Win64.zip')
New-Item -ItemType Directory -Path $outputFolder -Force | Out-Null

Write-Host "Unreal Engine: $EngineRoot"
Write-Host "Source plugin: $pluginFile"
Write-Host "Package folder: $packageFolder"
Write-Host 'Building plugin (Win64 editor)...'
& $runUAT BuildPlugin "-Plugin=$pluginFile" "-Package=$packageFolder" -TargetPlatforms=Win64 -Rocket
if ($LASTEXITCODE -ne 0) {
    throw "RunUAT BuildPlugin failed with exit code $LASTEXITCODE. No distributable package was produced."
}

$packagedDescriptor = Join-Path $packageFolder 'GitStatus.uplugin'
$binaryFolder = Join-Path $packageFolder 'Binaries\Win64'
if (-not (Test-Path -LiteralPath $packagedDescriptor -PathType Leaf)) {
    throw 'Build reported success, but the packaged plugin descriptor is missing.'
}
if (-not (Test-Path -LiteralPath $binaryFolder -PathType Container)) {
    throw 'Build reported success, but packaged Binaries/Win64 is missing.'
}
$dlls = @(Get-ChildItem -LiteralPath $binaryFolder -Recurse -Filter '*.dll' -File)
if ($dlls.Count -eq 0) {
    throw 'Build reported success, but no Win64 plugin DLL was produced.'
}

Compress-Archive -LiteralPath $packageFolder -DestinationPath $archivePath -CompressionLevel Optimal
Write-Host ''
Write-Host 'BUILD COMPLETE' -ForegroundColor Green
Write-Host "Found $($dlls.Count) plugin DLL(s):"
$dlls | ForEach-Object { Write-Host ('  ' + $_.Name) }
Write-Host "Packaged plugin: $packageFolder"
Write-Host "Distributable archive: $archivePath"
Write-Host 'To test, close Unreal and install the packaged GitStatus directory in your project.'
Write-Host 'The archive can also be supplied for committing verified binaries into the game repository.'
