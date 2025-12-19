# Docker Homelab Manager - Build Script
# Automates the build process for Windows

param(
    [string]$QtPath = "C:\Qt\6.6.0\msvc2019_64",
    [string]$VcpkgPath = "C:\vcpkg",
    [switch]$Clean,
    [switch]$SkipDeploy
)

$ErrorActionPreference = "Stop"

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "Docker Homelab Manager - Build Script" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

# Check prerequisites
Write-Host "[1/6] Checking prerequisites..." -ForegroundColor Yellow

# Check CMake
if (!(Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: CMake not found!" -ForegroundColor Red
    Write-Host "Install with: winget install Kitware.CMake" -ForegroundColor Yellow
    exit 1
}
$cmakeVersion = cmake --version | Select-Object -First 1
Write-Host "  ✓ CMake found: $cmakeVersion" -ForegroundColor Green

# Check compiler
$vsInstallPath = & "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" `
    -latest -property installationPath -ErrorAction SilentlyContinue
if (!$vsInstallPath) {
    Write-Host "ERROR: Visual Studio not found!" -ForegroundColor Red
    Write-Host "Install with: winget install Microsoft.VisualStudio.2022.Community" -ForegroundColor Yellow
    exit 1
}
Write-Host "  ✓ Visual Studio found: $vsInstallPath" -ForegroundColor Green

# Check Qt
if (!(Test-Path $QtPath)) {
    Write-Host "WARNING: Qt not found at $QtPath" -ForegroundColor Yellow
    $QtPath = Read-Host "Enter Qt installation path (e.g., C:\Qt\6.6.0\msvc2019_64)"
    if (!(Test-Path $QtPath)) {
        Write-Host "ERROR: Invalid Qt path!" -ForegroundColor Red
        exit 1
    }
}
Write-Host "  ✓ Qt found: $QtPath" -ForegroundColor Green

# Check vcpkg (optional)
$useVcpkg = $false
if (Test-Path "$VcpkgPath\vcpkg.exe") {
    Write-Host "  ✓ vcpkg found: $VcpkgPath" -ForegroundColor Green
    $useVcpkg = $true
} else {
    Write-Host "  ℹ vcpkg not found (optional)" -ForegroundColor Gray
}

# Clean old build
Write-Host "`n[2/6] Preparing build directory..." -ForegroundColor Yellow
if ($Clean -and (Test-Path build)) {
    Write-Host "  Removing old build directory..." -ForegroundColor Gray
    Remove-Item build -Recurse -Force
}

if (!(Test-Path build)) {
    New-Item -ItemType Directory -Path build | Out-Null
}
Write-Host "  ✓ Build directory ready" -ForegroundColor Green

# Configure
Write-Host "`n[3/6] Configuring with CMake..." -ForegroundColor Yellow
Push-Location build

$cmakeArgs = @(
    ".."
    "-DCMAKE_PREFIX_PATH=`"$QtPath`""
    "-G", "Visual Studio 17 2022"
    "-A", "x64"
)

if ($useVcpkg) {
    $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=`"$VcpkgPath\scripts\buildsystems\vcpkg.cmake`""
}

Write-Host "  Running: cmake $($cmakeArgs -join ' ')" -ForegroundColor Gray
& cmake $cmakeArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "`nERROR: CMake configuration failed!" -ForegroundColor Red
    Write-Host "Check the error messages above for details." -ForegroundColor Yellow
    Pop-Location
    exit 1
}
Write-Host "  ✓ Configuration successful" -ForegroundColor Green

# Build
Write-Host "`n[4/6] Building (this may take 2-5 minutes)..." -ForegroundColor Yellow
$buildStart = Get-Date
cmake --build . --config Release --verbose

if ($LASTEXITCODE -ne 0) {
    Write-Host "`nERROR: Build failed!" -ForegroundColor Red
    Write-Host "Check the error messages above for details." -ForegroundColor Yellow
    Pop-Location
    exit 1
}
$buildEnd = Get-Date
$buildTime = ($buildEnd - $buildStart).TotalSeconds
Write-Host "  ✓ Build successful (took $([math]::Round($buildTime, 1))s)" -ForegroundColor Green

# Deploy Qt dependencies
if (!$SkipDeploy) {
    Write-Host "`n[5/6] Deploying Qt dependencies..." -ForegroundColor Yellow
    $windeployqt = "$QtPath\bin\windeployqt.exe"

    if (Test-Path $windeployqt) {
        & $windeployqt "Release\DockerHomelabManager.exe" --no-translations
        Write-Host "  ✓ Qt dependencies deployed" -ForegroundColor Green
    } else {
        Write-Host "  WARNING: windeployqt not found, you may need to manually copy Qt DLLs" -ForegroundColor Yellow
    }
} else {
    Write-Host "`n[5/6] Skipping deployment (--SkipDeploy flag)" -ForegroundColor Gray
}

Pop-Location

# Verify
Write-Host "`n[6/6] Verifying build..." -ForegroundColor Yellow
$exePath = "build\Release\DockerHomelabManager.exe"
if (Test-Path $exePath) {
    $fileSize = (Get-Item $exePath).Length / 1MB
    Write-Host "  ✓ Executable found: $exePath" -ForegroundColor Green
    Write-Host "  ✓ Size: $([math]::Round($fileSize, 2)) MB" -ForegroundColor Green
} else {
    Write-Host "  ERROR: Executable not found at $exePath" -ForegroundColor Red
    exit 1
}

# Success
Write-Host "`n========================================" -ForegroundColor Green
Write-Host "BUILD SUCCESSFUL!" -ForegroundColor Green -NoNewline
Write-Host " 🚀" -ForegroundColor White
Write-Host "========================================" -ForegroundColor Green

Write-Host "`nExecutable location:" -ForegroundColor Cyan
Write-Host "  $exePath" -ForegroundColor White

Write-Host "`nRun the application:" -ForegroundColor Cyan
Write-Host "  .\$exePath" -ForegroundColor White
Write-Host "  or" -ForegroundColor Gray
Write-Host "  cd build\Release && .\DockerHomelabManager.exe" -ForegroundColor White

Write-Host "`nMake sure Docker is running before starting the application!`n" -ForegroundColor Yellow
