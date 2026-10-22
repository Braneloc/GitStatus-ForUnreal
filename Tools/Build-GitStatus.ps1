# GitStatus Windows plugin packaging helper.
# Packages from a clean source staging directory so previously installed
# binary-only builds or Unreal build leftovers cannot enter the new package.
[CmdletBinding()]
param(
    [string]$EngineRoot,
    [string]$EngineVersion
)

$ErrorActionPreference = 'Stop'

$pluginRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $pluginRoot '..\..')).Path
$installedDescriptor = Join-Path $pluginRoot 'GitStatus.uplugin'

if (-not (Test-Path -LiteralPath $installedDescriptor -PathType Leaf)) {
    throw "GitStatus.uplugin not found: $installedDescriptor"
}

$descriptor = Get-Content -LiteralPath $installedDescriptor -Raw | ConvertFrom-Json
if (-not ($descriptor.Modules | Where-Object { $_.Name -eq 'GitStatusEditor' })) {
    throw 'GitStatus.uplugin does not declare the GitStatusEditor module.'
}

$projectFile = Get-ChildItem -LiteralPath $projectRoot -Filter '*.uproject' -File -ErrorAction SilentlyContinue |
    Select-Object -First 1

if ([string]::IsNullOrWhiteSpace($EngineVersion)) {
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
    throw "Unreal Engine's RunUAT.bat was not found at: $runUAT. Use -EngineRoot to specify your installation."
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmssfff'
# Keep new packages outside the installed plugin so that replacing the old
# plugin directory cannot accidentally delete the freshly built output.
if ($projectFile) {
    $buildOutputRoot = Join-Path $projectRoot 'Saved\GitStatusBuildOutput'
}
else {
    $buildOutputRoot = Join-Path $pluginRoot 'BuildOutput'
}
$outputFolder = Join-Path $buildOutputRoot ('GitStatus-UE' + $EngineVersion + '-Win64-' + $stamp)
$checkoutFolder = Join-Path $outputFolder 'SourceCheckout'
$stageFolder = Join-Path $outputFolder 'SourcePlugin'
$packageFolder = Join-Path $outputFolder 'GitStatus'
$archivePath = Join-Path $outputFolder ('GitStatus-UE' + $EngineVersion + '-Win64.zip')

$requiredSource = @(
    'Source\GitStatusEditor\GitStatusEditor.Build.cs',
    'Source\GitStatusEditor\Private\GitStatusEditorModule.cpp',
    'Source\GitStatusEditor\Private\GitStatusEditorModule.h'
)
$missing = @($requiredSource | Where-Object {
    -not (Test-Path -LiteralPath (Join-Path $pluginRoot $_) -PathType Leaf)
})
$sourceRoot = $pluginRoot
$downloaded = $false

if ($missing.Count -gt 0) {
    # A packaged binary-only plugin cannot be used as input to BuildPlugin.
    # Its Source/ folder is intentionally absent. Obtain the official source
    # in a separate checkout; do not change the installed 5.7/5.8 plugin.
    $existingModuleFiles = @($requiredSource | Where-Object {
        Test-Path -LiteralPath (Join-Path $pluginRoot $_) -PathType Leaf
    })
    if ($existingModuleFiles.Count -gt 0) {
        throw ("Incomplete local GitStatus source. Missing: " + ($missing -join ', ') +
            ". Restore the files from https://github.com/Braneloc/GitStatus-ForUnreal before building.")
    }

    Write-Host 'Binary-only GitStatus installation detected (no Source module).' -ForegroundColor Yellow
    Write-Host 'Fetching a clean copy of the GitStatus source for this build.'
    $git = Get-Command git -ErrorAction SilentlyContinue
    if (-not $git) {
        throw 'Git is required to fetch missing plugin source. Install Git, or copy the full upstream Source folder to Plugins/GitStatus/Source.'
    }

    New-Item -ItemType Directory -Path $outputFolder -Force | Out-Null
    & $git.Source clone --quiet --depth 1 --branch main -- 'https://github.com/Braneloc/GitStatus-ForUnreal.git' $checkoutFolder
    if ($LASTEXITCODE -ne 0) {
        throw "Cannot fetch GitStatus source (git exit $LASTEXITCODE). Ensure GitHub is reachable."
    }

    $sourceRoot = $checkoutFolder
    $downloaded = $true
}

foreach ($relativePath in $requiredSource) {
    if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot $relativePath) -PathType Leaf)) {
        throw "Missing module source file: $(Join-Path $sourceRoot $relativePath). Cannot package."
    }
}

# Always build from a clean staging copy outside the installed plugin's
# directories. Excludes Binaries/, Intermediate/, Distributions/, BuildOutput/
# and .git/ from the source input, including any incompatible 5.7 DLL.
New-Item -ItemType Directory -Path $stageFolder -Force | Out-Null
foreach ($file in @('GitStatus.uplugin', 'README.md')) {
    $sourcePath = Join-Path $sourceRoot $file
    if (Test-Path -LiteralPath $sourcePath -PathType Leaf) {
        Copy-Item -LiteralPath $sourcePath -Destination $stageFolder -Force
    }
}
foreach ($folder in @('Source', 'Resources')) {
    $sourcePath = Join-Path $sourceRoot $folder
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Container)) {
        throw "GitStatus $folder directory not found at $sourcePath"
    }
    Copy-Item -LiteralPath $sourcePath -Destination $stageFolder -Recurse -Force
}

$stagedDescriptor = Join-Path $stageFolder 'GitStatus.uplugin'
if (-not (Test-Path -LiteralPath $stagedDescriptor -PathType Leaf)) {
    throw "No source plugin descriptor was staged at $stagedDescriptor"
}

# The descriptor from a binary-only 5.7 package can include EngineVersion
# metadata. The staged source descriptor must be engine-independent.
$stagedJson = Get-Content -LiteralPath $stagedDescriptor -Raw | ConvertFrom-Json
if ($stagedJson.PSObject.Properties.Name -contains 'EngineVersion') {
    $stagedJson.PSObject.Properties.Remove('EngineVersion')
}
$stagedJson.Installed = $false
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($stagedDescriptor, ($stagedJson | ConvertTo-Json -Depth 20) + [Environment]::NewLine, $utf8NoBom)

Write-Host "Unreal Engine: $EngineRoot"
Write-Host "Installed plugin: $pluginRoot"
Write-Host "Build source: $sourceRoot"
Write-Host "Source recovered from GitHub: $downloaded"
Write-Host "Staged plugin: $stagedDescriptor"
Write-Host "Package folder: $packageFolder"
Write-Host 'Building GitStatus editor plugin for Win64...'

& $runUAT BuildPlugin "-Plugin=$stagedDescriptor" "-Package=$packageFolder" -TargetPlatforms=Win64 -Rocket
if ($LASTEXITCODE -ne 0) {
    throw "RunUAT BuildPlugin failed with exit code $LASTEXITCODE. Build staging and logs remain in $outputFolder"
}

$packagedDescriptor = Join-Path $packageFolder 'GitStatus.uplugin'
$binaryFolder = Join-Path $packageFolder 'Binaries\Win64'
if (-not (Test-Path -LiteralPath $packagedDescriptor -PathType Leaf)) {
    throw 'Build reported success but the packaged plugin descriptor is missing.'
}
if (-not (Test-Path -LiteralPath $binaryFolder -PathType Container)) {
    throw 'Build reported success but Binaries/Win64 is missing.'
}
$dlls = @(Get-ChildItem -LiteralPath $binaryFolder -Recurse -Filter '*.dll' -File)
if ($dlls.Count -eq 0) {
    throw 'Build reported success but no Win64 plugin DLL was produced.'
}

# Remove only our own temporary staging directories after a successful build.
# Never touch the plugin currently installed in an Unreal project.
if (Test-Path -LiteralPath $checkoutFolder) {
    Remove-Item -LiteralPath $checkoutFolder -Recurse -Force
}
if (Test-Path -LiteralPath $stageFolder) {
    Remove-Item -LiteralPath $stageFolder -Recurse -Force
}

Compress-Archive -LiteralPath $packageFolder -DestinationPath $archivePath -CompressionLevel Optimal

Write-Host ''
Write-Host 'BUILD COMPLETE' -ForegroundColor Green
Write-Host "Found $($dlls.Count) Win64 plugin DLL(s):"
$dlls | ForEach-Object { Write-Host ('  ' + $_.Name) }
Write-Host "Packaged plugin: $packageFolder"
Write-Host "Distributable ZIP: $archivePath"
Write-Host 'Close Unreal before installing the new package. Preserve local plugin edits.'
