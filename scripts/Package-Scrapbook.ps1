param(
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release",
    [string]$BuildDir = "build",
    [string]$OutputDir = "dist",
    [string]$QtRoot = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$launcherScript = Join-Path $PSScriptRoot "Run-Scrapbook.ps1"
$buildPath = Join-Path $repoRoot $BuildDir
$runtimeDir = Join-Path $buildPath "bin\$Configuration"
$packageRoot = Join-Path $repoRoot $OutputDir
$packageDir = Join-Path $packageRoot "Scrapbook-$Configuration"
$zipPath = Join-Path $packageRoot "Scrapbook-$Configuration.zip"

if ($QtRoot) {
    & $launcherScript -Configuration $Configuration -NoRun -QtRoot $QtRoot
} else {
    & $launcherScript -Configuration $Configuration -NoRun
}

if ($LASTEXITCODE -ne 0) {
    throw "Build/deploy step failed."
}

if (-not (Test-Path $runtimeDir)) {
    throw "Runtime directory not found: $runtimeDir"
}

if (Test-Path $packageDir) {
    Remove-Item -Recurse -Force $packageDir
}

New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null
Copy-Item -Recurse -Force $runtimeDir $packageDir

if (Test-Path $zipPath) {
    Remove-Item -Force $zipPath
}

Compress-Archive -Path (Join-Path $packageDir "*") -DestinationPath $zipPath

Write-Host "> packaged runtime folder: $packageDir"
Write-Host "> packaged zip: $zipPath"
