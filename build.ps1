#!/usr/bin/env powershell
<#
.SYNOPSIS
    Build script for Hydra library using CMake

.PARAMETER Configuration
    Build configuration (Debug or Release). Default: Debug

.PARAMETER Clean
    Clean build directory before building

.PARAMETER Generator
    CMake generator to use. Default: "Visual Studio 17 2022"

.PARAMETER Toolset
    CMake toolset to use. Default: "ClangCL"

.PARAMETER Jobs
    Number of parallel jobs for building. Default: automatic

.EXAMPLE
    .\build.ps1
    .\build.ps1 -Configuration Release
    .\build.ps1 -Clean -Configuration Release -Jobs 8
#>

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [Parameter(Mandatory=$false)]
    [switch]$Clean,

    [Parameter(Mandatory=$false)]
    [string]$Generator = "Visual Studio 17 2022",

    [Parameter(Mandatory=$false)]
    [string]$Toolset = "ClangCL",

    [Parameter(Mandatory=$false)]
    [int]$Jobs = 0
)

# Set error action preference
$ErrorActionPreference = "Stop"

# Get script directory and project root
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$BuildDir = Join-Path $ProjectRoot "build"

Write-Host "Building Hydra Library" -ForegroundColor Green
Write-Host "Configuration: $Configuration" -ForegroundColor Yellow
Write-Host "Generator: $Generator" -ForegroundColor Yellow
Write-Host "Toolset: $Toolset" -ForegroundColor Yellow
Write-Host "Project Root: $ProjectRoot" -ForegroundColor Yellow

# Clean if requested
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

# Create build directory
if (!(Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

# Change to build directory
Push-Location $BuildDir

try {
    # Configure with CMake
    Write-Host "Configuring with CMake..." -ForegroundColor Yellow
    $cmakeArgs = @(
        "-G", $Generator,
        "-A", "x64",
        "-T", $Toolset,
        "-DCMAKE_BUILD_TYPE=$Configuration",
        $ProjectRoot
    )

    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed with exit code $LASTEXITCODE"
    }

    # Build
    Write-Host "Building..." -ForegroundColor Yellow
    $buildArgs = @("--build", ".", "--config", $Configuration)

    if ($Jobs -gt 0) {
        $buildArgs += @("--parallel", $Jobs)
    } else {
        $buildArgs += "--parallel"
    }

    & cmake @buildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }

    Write-Host "Build completed successfully!" -ForegroundColor Green

    # Show output information
    $outputPath = Join-Path $BuildDir "$Configuration\libhydra.lib"
    if (Test-Path $outputPath) {
        $fileInfo = Get-Item $outputPath
        Write-Host "Library built: $($fileInfo.Name)" -ForegroundColor Cyan
        Write-Host "Size: $([math]::Round($fileInfo.Length / 1KB, 2)) KB" -ForegroundColor Cyan
        Write-Host "Location: $($fileInfo.FullName)" -ForegroundColor Cyan
    }

} catch {
    Write-Host "Build failed: $_" -ForegroundColor Red
    exit 1
} finally {
    Pop-Location
}
