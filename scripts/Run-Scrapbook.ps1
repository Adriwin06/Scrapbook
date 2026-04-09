param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [switch]$NoRun,
    [string]$BuildDir = "build",
    [string]$QtRoot = ""
)

$ErrorActionPreference = "Stop"

function Resolve-RepoRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

function Resolve-QtRoot {
    param([string]$RequestedRoot)

    function Test-QtKit {
        param([string]$Candidate)

        if (-not $Candidate) {
            return $null
        }

        try {
            $resolved = (Resolve-Path $Candidate -ErrorAction Stop).Path
        } catch {
            return $null
        }

        $qt6Config = Join-Path $resolved "lib\cmake\Qt6\Qt6Config.cmake"
        $windeployqt = Join-Path $resolved "bin\windeployqt.exe"
        if ((Test-Path $qt6Config) -and (Test-Path $windeployqt)) {
            return $resolved
        }

        return $null
    }

    function Get-QtRootFromQt6Dir {
        param([string]$Qt6DirPath)

        if (-not $Qt6DirPath) {
            return $null
        }

        try {
            $resolvedQt6Dir = (Resolve-Path $Qt6DirPath -ErrorAction Stop).Path
        } catch {
            return $null
        }

        $candidate = Join-Path $resolvedQt6Dir "..\..\.."
        return Test-QtKit $candidate
    }

    function Get-QtRootFromPathTool {
        param([string]$ToolName)

        try {
            $command = Get-Command $ToolName -ErrorAction Stop | Select-Object -First 1
        } catch {
            return $null
        }

        $toolPath = Split-Path $command.Source -Parent
        $candidate = Join-Path $toolPath ".."
        return Test-QtKit $candidate
    }

    if ($RequestedRoot) {
        $candidate = Test-QtKit $RequestedRoot
        if ($candidate) {
            return $candidate
        }
        throw "The supplied Qt root is not a valid Qt desktop kit: $RequestedRoot"
    }

    if ($env:SCRAPBOOK_QT_ROOT) {
        $candidate = Test-QtKit $env:SCRAPBOOK_QT_ROOT
        if ($candidate) {
            return $candidate
        }
    }

    if ($env:Qt6_DIR) {
        $candidate = Get-QtRootFromQt6Dir $env:Qt6_DIR
        if ($candidate) {
            return $candidate
        }
    }

    if ($env:CMAKE_PREFIX_PATH) {
        foreach ($entry in ($env:CMAKE_PREFIX_PATH -split ';')) {
            $candidate = Test-QtKit $entry
            if ($candidate) {
                return $candidate
            }
            $candidate = Get-QtRootFromQt6Dir (Join-Path $entry "lib\cmake\Qt6")
            if ($candidate) {
                return $candidate
            }
        }
    }

    foreach ($toolName in @("windeployqt", "qmake", "qtpaths")) {
        $candidate = Get-QtRootFromPathTool $toolName
        if ($candidate) {
            return $candidate
        }
    }

    $kits = Get-ChildItem "C:\Qt" -Directory -ErrorAction SilentlyContinue |
        ForEach-Object {
            $kitPath = Join-Path $_.FullName "msvc2022_64"
            $configPath = Join-Path $kitPath "lib\cmake\Qt6\Qt6Config.cmake"
            if (Test-Path $configPath) {
                [PSCustomObject]@{
                    Version = [version]$_.Name
                    Path = $kitPath
                }
            }
        } |
        Sort-Object Version -Descending

    if (-not $kits) {
        throw "Could not locate a usable Qt desktop kit. Set SCRAPBOOK_QT_ROOT, Qt6_DIR, or CMAKE_PREFIX_PATH, or put qmake/windeployqt on PATH."
    }

    return $kits[0].Path
}

function Invoke-Step {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    Write-Host "> $FilePath $($Arguments -join ' ')"
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $FilePath"
    }
}

$repoRoot = Resolve-RepoRoot
$qtRoot = Resolve-QtRoot -RequestedRoot $QtRoot
$qt6Dir = Join-Path $qtRoot "lib\cmake\Qt6"
$qtBinDir = Join-Path $qtRoot "bin"
$windeployqt = Join-Path $qtBinDir "windeployqt.exe"
$buildPath = Join-Path $repoRoot $BuildDir
$qmlDir = Join-Path $repoRoot "apps\scrapbook\qml"
$appExe = Join-Path $buildPath "bin\$Configuration\scrapbook.exe"
$cachePath = Join-Path $buildPath "CMakeCache.txt"

if (-not (Test-Path $qt6Dir)) {
    throw "Qt6_DIR does not exist: $qt6Dir"
}

if (-not (Test-Path $windeployqt)) {
    throw "windeployqt.exe was not found at: $windeployqt"
}

$env:CMAKE_PREFIX_PATH = $qtRoot
$env:PATH = "$qtBinDir;$env:PATH"

if (Test-Path $cachePath) {
    $cache = Get-Content $cachePath -ErrorAction SilentlyContinue
    $generatorLine = $cache | Where-Object { $_ -like "CMAKE_GENERATOR:INTERNAL=*" } | Select-Object -First 1
    $platformLine = $cache | Where-Object { $_ -like "CMAKE_GENERATOR_PLATFORM:INTERNAL=*" } | Select-Object -First 1
    $generator = if ($generatorLine) { $generatorLine.Split("=", 2)[1] } else { "" }
    $platform = if ($platformLine) { $platformLine.Split("=", 2)[1] } else { "" }

    if ($generator -ne "Visual Studio 17 2022" -or ($platform -and $platform -ne "x64")) {
        Write-Host "> removing incompatible build directory $buildPath"
        Remove-Item -Recurse -Force $buildPath
    }
}

Invoke-Step -FilePath "cmake" -Arguments @(
    "-S", $repoRoot,
    "-B", $buildPath,
    "-G", "Visual Studio 17 2022",
    "-A", "x64",
    "-DQt6_DIR=$qt6Dir"
)

Invoke-Step -FilePath "cmake" -Arguments @(
    "--build", $buildPath,
    "--config", $Configuration,
    "--target", "scrapbook", "scrapbook_pipeline", "libatlas_tool"
)

$deployArgs = @(
    "--qmldir", $qmlDir,
    "--compiler-runtime",
    "--no-translations"
)

if ($Configuration -eq "Debug") {
    $deployArgs += "--debug"
} else {
    $deployArgs += "--release"
}

$deployArgs += $appExe
Invoke-Step -FilePath $windeployqt -Arguments $deployArgs

if (-not (Test-Path $appExe)) {
    throw "Built application not found: $appExe"
}

if (-not $NoRun) {
    Write-Host "> launching $appExe"
    Start-Process -FilePath $appExe -WorkingDirectory (Split-Path $appExe)
}
