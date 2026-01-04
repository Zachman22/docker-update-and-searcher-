# Docker Homelab Manager - Windows Build Guide

## 🪟 Building on Windows

This guide provides step-by-step instructions for building Docker Homelab Manager v0.4.0 on Windows.

---

## 📋 Prerequisites

- **Windows 10/11** (64-bit)
- **Visual Studio 2019 or later** (Community Edition is free)
- **CMake** >= 3.16
- **Git** for Windows
- **Administrator privileges** (for vcpkg installation)

---

## 🚀 Option 1: Build with vcpkg (Recommended)

vcpkg is Microsoft's C++ package manager and provides the easiest way to get all dependencies.

### Step 1: Install vcpkg

```powershell
# Open PowerShell as Administrator
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# Add vcpkg to your PATH (optional but recommended)
$env:PATH += ";C:\vcpkg"
[Environment]::SetEnvironmentVariable("Path", $env:PATH, [System.EnvironmentVariableTarget]::User)
```

### Step 2: Install Dependencies

```powershell
# This will take 30-60 minutes on first run (Qt6 is large)
.\vcpkg install qt6-base:x64-windows
.\vcpkg install qt6-network:x64-windows
.\vcpkg install sqlite3:x64-windows
.\vcpkg install curl:x64-windows

# Wait for installation to complete...
```

### Step 3: Configure CMake

```powershell
# Navigate to your project directory
cd C:\Users\YourName\docker-update-and-searcher-

# Configure with vcpkg toolchain
cmake -B build -S . `
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DCMAKE_BUILD_TYPE=Release
```

### Step 4: Build

```powershell
# Build the project (uses all CPU cores)
cmake --build build --config Release --parallel

# The executable will be at:
# build\Release\DockerHomelabManager.exe
```

### Step 5: Deploy Qt DLLs

```powershell
# Copy required Qt DLLs to exe directory
cd build\Release
windeployqt DockerHomelabManager.exe

# Now you can run:
.\DockerHomelabManager.exe
```

---

## 🔧 Option 2: Build with Qt Online Installer

If you prefer the official Qt installer:

### Step 1: Download Qt

1. Go to https://www.qt.io/download-qt-installer
2. Download **Qt Online Installer for Windows**
3. Run the installer

### Step 2: Install Qt6

1. Create a Qt account (free)
2. Select **Custom Installation**
3. Choose **Qt 6.6 or later**
4. Select components:
   - Qt 6.x for Windows (MSVC 2019 64-bit)
   - Qt Network
   - CMake (if not already installed)
5. Install to `C:\Qt` (default)

### Step 3: Install Other Dependencies

**SQLite3:**
```powershell
# Use vcpkg for other dependencies
C:\vcpkg\vcpkg install sqlite3:x64-windows curl:x64-windows
```

**Or download manually:**
- SQLite: https://www.sqlite.org/download.html (amalgamation + tools)
- CURL: https://curl.se/windows/ (Win64 binary)

### Step 4: Configure with Qt Path

```powershell
cd C:\Users\YourName\docker-update-and-searcher-

cmake -B build -S . `
  -DCMAKE_PREFIX_PATH=C:\Qt\6.6.0\msvc2019_64 `
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DCMAKE_BUILD_TYPE=Release
```

### Step 5: Build

```powershell
cmake --build build --config Release --parallel
```

---

## 🛠️ Troubleshooting

### Issue: "CMake not found"

**Solution:**
```powershell
# Download CMake from https://cmake.org/download/
# Or install via chocolatey:
choco install cmake
```

### Issue: "Visual Studio not found"

**Solution:**
```powershell
# Install Visual Studio 2022 Community (free)
# https://visualstudio.microsoft.com/downloads/
# Make sure to select "Desktop development with C++"
```

### Issue: "Qt6 not found"

**Solution:**
```powershell
# Verify Qt6 installed correctly:
dir C:\vcpkg\installed\x64-windows\bin\Qt6*.dll

# Or check Qt installation:
dir C:\Qt\6.6.0\msvc2019_64\bin\Qt6*.dll

# If empty, reinstall Qt6 via vcpkg:
C:\vcpkg\vcpkg remove qt6-base:x64-windows
C:\vcpkg\vcpkg install qt6-base:x64-windows --recurse
```

### Issue: "vcpkg integration failed"

**Solution:**
```powershell
# Integrate vcpkg with Visual Studio:
C:\vcpkg\vcpkg integrate install

# Then rebuild:
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

### Issue: Build fails with "error C2039"

**Solution:**
This usually means C++17 is not enabled. Add to CMake:
```powershell
cmake -B build -S . `
  -DCMAKE_CXX_STANDARD=17 `
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

### Issue: Missing DLLs when running

**Solution:**
```powershell
# Run windeployqt to copy all required DLLs:
cd build\Release
C:\Qt\6.6.0\msvc2019_64\bin\windeployqt.exe DockerHomelabManager.exe

# Or with vcpkg:
C:\vcpkg\installed\x64-windows\tools\qt6\bin\windeployqt.exe DockerHomelabManager.exe
```

---

## 📦 Creating a Portable Package

Once built successfully:

```powershell
# Create distribution folder
mkdir DockerHomelabManager-v0.4.0-Windows
cd DockerHomelabManager-v0.4.0-Windows

# Copy executable
copy ..\build\Release\DockerHomelabManager.exe .

# Copy Qt DLLs
C:\Qt\6.6.0\msvc2019_64\bin\windeployqt.exe DockerHomelabManager.exe

# Copy additional DLLs from vcpkg
copy C:\vcpkg\installed\x64-windows\bin\sqlite3.dll .
copy C:\vcpkg\installed\x64-windows\bin\libcurl.dll .

# Copy Visual C++ Runtime (if needed)
copy "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\14.xx.xxxxx\x64\Microsoft.VC143.CRT\*.dll" .

# Create zip
Compress-Archive -Path * -DestinationPath ..\DockerHomelabManager-v0.4.0-Windows-x64.zip
```

---

## ⚡ Quick Build Script

Save this as `build-windows.ps1`:

```powershell
# Docker Homelab Manager - Windows Build Script
# Run with: .\build-windows.ps1

$ErrorActionPreference = "Stop"

Write-Host "Docker Homelab Manager v0.4.0 - Windows Build Script" -ForegroundColor Cyan
Write-Host "======================================================" -ForegroundColor Cyan

# Check if vcpkg exists
if (-not (Test-Path "C:\vcpkg\vcpkg.exe")) {
    Write-Host "ERROR: vcpkg not found at C:\vcpkg" -ForegroundColor Red
    Write-Host "Please install vcpkg first (see WINDOWS_BUILD_GUIDE.md)" -ForegroundColor Yellow
    exit 1
}

# Check dependencies
Write-Host "`nChecking dependencies..." -ForegroundColor Yellow
$deps = @("qt6-base:x64-windows", "sqlite3:x64-windows", "curl:x64-windows")
foreach ($dep in $deps) {
    $installed = C:\vcpkg\vcpkg list | Select-String $dep
    if (-not $installed) {
        Write-Host "Installing $dep..." -ForegroundColor Yellow
        C:\vcpkg\vcpkg install $dep
    } else {
        Write-Host "✓ $dep already installed" -ForegroundColor Green
    }
}

# Clean build directory
if (Test-Path "build") {
    Write-Host "`nCleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force build
}

# Configure
Write-Host "`nConfiguring CMake..." -ForegroundColor Yellow
cmake -B build -S . `
    -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
    -DCMAKE_BUILD_TYPE=Release

if ($LASTEXITCODE -ne 0) {
    Write-Host "`nERROR: CMake configuration failed!" -ForegroundColor Red
    exit 1
}

# Build
Write-Host "`nBuilding..." -ForegroundColor Yellow
cmake --build build --config Release --parallel

if ($LASTEXITCODE -ne 0) {
    Write-Host "`nERROR: Build failed!" -ForegroundColor Red
    exit 1
}

# Deploy Qt DLLs
Write-Host "`nDeploying Qt dependencies..." -ForegroundColor Yellow
$qtDeployTool = "C:\vcpkg\installed\x64-windows\tools\qt6\bin\windeployqt.exe"
if (Test-Path $qtDeployTool) {
    & $qtDeployTool build\Release\DockerHomelabManager.exe
} else {
    Write-Host "Warning: windeployqt not found, you may need to copy Qt DLLs manually" -ForegroundColor Yellow
}

# Success
Write-Host "`n======================================================" -ForegroundColor Green
Write-Host "BUILD SUCCESSFUL!" -ForegroundColor Green
Write-Host "======================================================" -ForegroundColor Green
Write-Host "`nExecutable location:" -ForegroundColor Cyan
Write-Host "  build\Release\DockerHomelabManager.exe" -ForegroundColor White
Write-Host "`nTo run:" -ForegroundColor Cyan
Write-Host "  cd build\Release" -ForegroundColor White
Write-Host "  .\DockerHomelabManager.exe" -ForegroundColor White
```

---

## 🎯 Expected Build Output

When successful, you should see:

```
[100%] Built target DockerHomelabManager
```

And the file structure:

```
build\Release\
├── DockerHomelabManager.exe  (~15-25 MB)
├── Qt6Core.dll
├── Qt6Widgets.dll
├── Qt6Network.dll
├── sqlite3.dll
├── libcurl.dll
└── [other Qt plugins and DLLs]
```

---

## 📊 Build Time Estimates

| Task | Time |
|------|------|
| vcpkg installation | 5 minutes |
| Qt6 + dependencies download | 30-60 minutes |
| CMake configuration | 1-2 minutes |
| Compilation | 5-10 minutes |
| **Total (first time)** | **45-80 minutes** |
| **Subsequent builds** | **5-10 minutes** |

---

## ✅ Verification

After building:

```powershell
# Check executable exists
dir build\Release\DockerHomelabManager.exe

# Check size (should be 15-25 MB)
(Get-Item build\Release\DockerHomelabManager.exe).Length / 1MB

# Check DLL dependencies
dumpbin /DEPENDENTS build\Release\DockerHomelabManager.exe | Select-String "Qt6"

# Run the application
cd build\Release
.\DockerHomelabManager.exe
```

---

## 🐛 Getting Help

If you encounter issues:

1. Check the error message carefully
2. Verify all dependencies installed: `C:\vcpkg\vcpkg list`
3. Try clean rebuild: `Remove-Item -Recurse -Force build`
4. Check CMake version: `cmake --version` (should be >= 3.16)
5. Review build logs in `build\` directory

---

## 📞 Additional Resources

- **vcpkg docs:** https://vcpkg.io/
- **Qt documentation:** https://doc.qt.io/qt-6/
- **CMake tutorial:** https://cmake.org/cmake/help/latest/guide/tutorial/
- **Project issues:** https://github.com/Zachman22/docker-update-and-searcher-/issues

---

**Last Updated:** 2026-01-03
**Platform:** Windows 10/11 (64-bit)
**Status:** Ready to build
**Version:** v0.4.0
