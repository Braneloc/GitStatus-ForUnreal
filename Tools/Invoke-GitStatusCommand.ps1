<#
.SYNOPSIS
    Sends a safe local editor command to the GitStatus plugin and awaits its result.
.EXAMPLE
    .\Tools\Invoke-GitStatusCommand.ps1 -ProjectRoot 'D:\Unreal\DragonCare' -Action SaveFile -AssetPath 'Content\Maps\Arena.umap'
.EXAMPLE
    .\Tools\Invoke-GitStatusCommand.ps1 -ProjectRoot 'D:\Unreal\DragonCare' -Action SaveAndShutdown
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectRoot,

    [Parameter(Mandatory = $true)]
    [ValidateSet('SaveFile', 'SaveCurrentLevel', 'SaveAll', 'Shutdown', 'SaveAndShutdown')]
    [string]$Action,

    [string]$AssetPath = '',

    [ValidateRange(5, 60)]
    [int]$TimeoutSeconds = 45
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ($Action -eq 'SaveFile' -and [string]::IsNullOrWhiteSpace($AssetPath)) {
    throw 'SaveFile requires -AssetPath, such as /Game/Maps/Arena or Content\Maps\Arena.umap.'
}

$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).ProviderPath
$ProjectFiles = @(Get-ChildItem -LiteralPath $ProjectRoot -Filter '*.uproject' -File)
if ($ProjectFiles.Count -ne 1) {
    throw "Expected one .uproject in '$ProjectRoot'; found $($ProjectFiles.Count)."
}

$ControlDirectory = Join-Path $ProjectRoot 'Saved\GitStatusControl'
$CommandsDirectory = Join-Path $ControlDirectory 'Commands'
$ResultsDirectory = Join-Path $ControlDirectory 'Results'
New-Item -ItemType Directory -Force -Path $CommandsDirectory, $ResultsDirectory | Out-Null

$RequestIdentifier = [guid]::NewGuid().ToString('N')
$CommandFile = Join-Path $CommandsDirectory "$RequestIdentifier.json"
$TemporaryFile = Join-Path $CommandsDirectory "$RequestIdentifier.tmp"
$ResultFile = Join-Path $ResultsDirectory "$RequestIdentifier.json"

$Request = @{
    Action = $Action
    AssetPath = $AssetPath
    CreatedUtc = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ', [Globalization.CultureInfo]::InvariantCulture)
} | ConvertTo-Json -Compress

# Atomic rename prevents Unreal from reading a partially written command.
[IO.File]::WriteAllText($TemporaryFile, $Request, (New-Object Text.UTF8Encoding($false)))
[IO.File]::Move($TemporaryFile, $CommandFile)

$Watch = [Diagnostics.Stopwatch]::StartNew()
while ($Watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
    if (Test-Path -LiteralPath $ResultFile) {
        try {
            $Response = Get-Content -LiteralPath $ResultFile -Raw -Encoding UTF8 | ConvertFrom-Json
            if ($Response.RequestIdentifier -eq $RequestIdentifier) {
                Remove-Item -LiteralPath $ResultFile -Force
                $Response | ConvertTo-Json -Compress
                if (-not $Response.Success) {
                    exit 1
                }
                exit 0
            }
        }
        catch {
            # The editor could still be writing the result. Try again shortly.
        }
    }
    Start-Sleep -Milliseconds 250
}

# Prevent a command timed out while the editor was closed from running on next launch.
if (Test-Path -LiteralPath $CommandFile) {
    Remove-Item -LiteralPath $CommandFile -Force
}
throw "Timed out waiting for Unreal Editor to respond to '$Action' after $TimeoutSeconds seconds. Is the editor running with the updated plugin?"
