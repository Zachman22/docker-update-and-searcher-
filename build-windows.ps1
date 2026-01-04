# Docker Homelab Manager v0.4.0 - Windows Build Script
# Author: Zachman22
# Date: 2026-01-03
#
# This script automates the build process on Windows
# Prerequisites: vcpkg installed at C:\vcpkg
#
# Usage: .\build-windows.ps1

param(
    [string]$VcpkgPath = "C:\vcpkg",
    [string]$BuildType = "Release",
    [switch]$Clean,
    [switch]$SkipDependencies
)

$ErrorActionPreference = "Stop"

# Colors
function Write-ColorOutput {
    param([string]$Message, [string]$Color = "White")
    Write-Host $Message -ForegroundColor $Color
}

function Write-Header {
    param([string]$Message)
    Write-Host ""
    Write-ColorOutput "======================================================" "Cyan"
    Write-ColorOutput $Message "Cyan"
    Write-ColorOutput "======================================================" "Cyan"
    Write-Host ""
}

function Write-Step {
    param([string]$Message)
    Write-ColorOutput "`n$Message" "Yellow"
}

function Write-Success {
    param([string]$Message)
    Write-ColorOutput "✓ $Message" "Green"
}

function Write-Error-Custom {
    param([string]$Message)
    Write-ColorOutput "✗ $Message" "Red"
}

# Main script
Write-Header "Docker Homelab Manager v0.4.0 - Windows Build Script"

# Check vcpkg installation
Write-Step "Checking vcpkg installation..."
if (-not (Test-Path "$VcpkgPath\vcpkg.exe")) {
    Write-Error-Custom "vcpkg not found at $VcpkgPath"
    Write-Host ""
    Write-Host "Please install vcpkg first:" -ForegroundColor Yellow
    Write-Host "  1. Open PowerShell as Administrator" -ForegroundColor White
    Write-Host "  2. Run: cd C:\" -ForegroundColor White
    Write-Host "  3. Run: git clone https://github.com/Microsoft/vcpkg.git" -ForegroundColor White
    Write-Host "  4. Run: cd vcpkg" -ForegroundColor White
    Write-Host "  5. Run: .\bootstrap-vcpkg.bat" -ForegroundColor White
    Write-Host ""
    exit 1
}
Write-Success "vcpkg found at $VcpkgPath"

# Check CMake
Write-Step "Checking CMake..."
try {
    $cmakeVersion = cmake --version 2>&1 | Select-String "version" | Out-String
    Write-Success "CMake installed: $($cmakeVersion.Trim())"
} catch {
    Write-Error-Custom "CMake not found in PATH"
    Write-Host "Download from: https://cmake.org/download/" -ForegroundColor Yellow
    exit 1
}

# Install dependencies
if (-not $SkipDependencies) {
    Write-Step "Checking/Installing dependencies..."

    $dependencies = @(
        "qt6-base:x64-windows",
        "qt6-network:x64-windows",
        "sqlite3:x64-windows",
        "curl:x64-windows"
    )

    foreach ($dep in $dependencies) {
        Write-Host "Checking $dep..." -NoNewline
        $installed = & $VcpkgPath\vcpkg list | Select-String $dep

        if ($installed) {
            Write-ColorOutput " [INSTALLED]" "Green"
        } else {
            Write-ColorOutput " [INSTALLING...]" "Yellow"
            Write-Host "This may take several minutes, especially for Qt6..." -ForegroundColor Cyan

            & $VcpkgPath\vcpkg install $dep

            if ($LASTEXITCODE -ne 0) {
                Write-Error-Custom "Failed to install $dep"
                exit 1
            }
            Write-Success "$dep installed successfully"
        }
    }

    Write-Success "All dependencies ready"
}

# Clean build directory
if ($Clean -and (Test-Path "build")) {
    Write-Step "Cleaning build directory..."
    Remove-Item -Recurse -Force build
    Write-Success "Build directory cleaned"
}

# Configure CMake
Write-Step "Configuring CMake..."
Write-Host "This will auto-fetch nlohmann/json and yaml-cpp..." -ForegroundColor Cyan

$cmakeArgs = @(
    "-B", "build",
    "-S", ".",
    "-DCMAKE_TOOLCHAIN_FILE=$VcpkgPath\scripts\buildsystems\vcpkg.cmake",
    "-DCMAKE_BUILD_TYPE=$BuildType"
)

cmake @cmakeArgs

if ($LASTEXITCODE -ne 0) {
    Write-Error-Custom "CMake configuration failed"
    Write-Host ""
    Write-Host "Common solutions:" -ForegroundColor Yellow
    Write-Host "  1. Ensure Qt6 is installed: $VcpkgPath\vcpkg install qt6-base:x64-windows" -ForegroundColor White
    Write-Host "  2. Try clean build: .\build-windows.ps1 -Clean" -ForegroundColor White
    Write-Host "  3. Check BUILD_STATUS_REPORT.md for details" -ForegroundColor White
    exit 1
}

Write-Success "CMake configuration successful"

# Build
Write-Step "Building project..."
Write-Host "Building with $BuildType configuration..." -ForegroundColor Cyan
Write-Host "Using all available CPU cores..." -ForegroundColor Cyan

cmake --build build --config $BuildType --parallel

if ($LASTEXITCODE -ne 0) {
    Write-Error-Custom "Build failed"
    Write-Host ""
    Write-Host "Check the build log above for specific errors" -ForegroundColor Yellow
    Write-Host "Common issues:" -ForegroundColor Yellow
    Write-Host "  - Missing header files: Check all files are present" -ForegroundColor White
    Write-Host "  - Syntax errors: Review recent changes" -ForegroundColor White
    Write-Host "  - Linker errors: Ensure all .cpp files are in CMakeLists.txt" -ForegroundColor White
    exit 1
}

Write-Success "Build completed successfully"

# Deploy Qt DLLs
Write-Step "Deploying Qt dependencies..."

$exePath = "build\$BuildType\DockerHomelabManager.exe"

if (-not (Test-Path $exePath)) {
    Write-Error-Custom "Executable not found at $exePath"
    exit 1
}

# Try to find windeployqt
$windeployqt = $null
$searchPaths = @(
    "$VcpkgPath\installed\x64-windows\tools\qt6\bin\windeployqt.exe",
    "C:\Qt\6.6.0\msvc2019_64\bin\windeployqt.exe",
    "C:\Qt\6.5.0\msvc2019_64\bin\windeployqt.exe"
)

foreach ($path in $searchPaths) {
    if (Test-Path $path) {
        $windeployqt = $path
        break
    }
}

if ($windeployqt) {
    Write-Host "Running windeployqt..." -ForegroundColor Cyan
    & $windeployqt $exePath --release --no-translations
    Write-Success "Qt dependencies deployed"
} else {
    Write-Host "Warning: windeployqt not found" -ForegroundColor Yellow
    Write-Host "You may need to copy Qt DLLs manually to run the application" -ForegroundColor Yellow
}

# Copy additional DLLs from vcpkg
Write-Step "Copying additional dependencies..."
$targetDir = "build\$BuildType"
$vcpkgBin = "$VcpkgPath\installed\x64-windows\bin"

$additionalDlls = @("sqlite3.dll", "libcurl.dll")
foreach ($dll in $additionalDlls) {
    $sourceDll = "$vcpkgBin\$dll"
    if (Test-Path $sourceDll) {
        Copy-Item $sourceDll $targetDir -Force
        Write-Success "Copied $dll"
    }
}

# Get executable info
$exeInfo = Get-Item $exePath
$exeSizeMB = [math]::Round($exeInfo.Length / 1MB, 2)

# Success summary
Write-Header "BUILD SUCCESSFUL!"

Write-ColorOutput "Build Summary:" "Cyan"
Write-Host "  Configuration: $BuildType" -ForegroundColor White
Write-Host "  Executable: $exePath" -ForegroundColor White
Write-Host "  Size: $exeSizeMB MB" -ForegroundColor White
Write-Host "  Built: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor White

Write-Host ""
Write-ColorOutput "To run the application:" "Cyan"
Write-Host "  cd build\$BuildType" -ForegroundColor White
Write-Host "  .\DockerHomelabManager.exe" -ForegroundColor White

Write-Host ""
Write-ColorOutput "To create a portable package:" "Cyan"
Write-Host "  cd build\$BuildType" -ForegroundColor White
Write-Host "  Compress-Archive -Path * -DestinationPath ..\..\DockerHomelabManager-Windows.zip" -ForegroundColor White

Write-Host ""
Write-Success "All done! 🚀"
Write-Host ""
