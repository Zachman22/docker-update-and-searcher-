# Building Docker Homelab Manager

## Prerequisites Installation (Windows)

### 1. Install CMake

**Option A: Using winget (Recommended)**
```powershell
winget install Kitware.CMake
```

**Option B: Download Installer**
- Visit: https://cmake.org/download/
- Download: Windows x64 Installer
- Run installer and **check "Add CMake to system PATH"**

### 2. Install Visual Studio Build Tools

**Option A: Visual Studio 2022 Community (Recommended)**
```powershell
winget install Microsoft.VisualStudio.2022.Community
```
Or download from: https://visualstudio.microsoft.com/downloads/

**During installation, select:**
- ✅ Desktop development with C++
- ✅ C++ CMake tools for Windows
- ✅ Windows 10/11 SDK

**Option B: Build Tools Only (Lighter)**
```powershell
winget install Microsoft.VisualStudio.2022.BuildTools
```

### 3. Install Qt6

**Option A: Using Qt Online Installer (Recommended)**
1. Download: https://www.qt.io/download-qt-installer
2. Run installer
3. Select Qt 6.x.x for MSVC 2019/2022 64-bit
4. Note installation path (e.g., `C:\Qt\6.6.0\msvc2019_64`)

**Option B: Using vcpkg**
```powershell
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
.\vcpkg install qt6-base:x64-windows
```

### 4. Install CURL and SQLite3

**Using vcpkg:**
```powershell
cd C:\vcpkg
.\vcpkg install curl:x64-windows sqlite3:x64-windows
```

---

## Building the Application

### Step 1: Open PowerShell/CMD as Administrator

```powershell
# Navigate to project
cd "C:\Users\Zach\Documents\docker updater and searcher"
```

### Step 2: Create Build Directory

```powershell
mkdir build
cd build
```

### Step 3: Configure with CMake

**If using vcpkg:**
```powershell
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_PREFIX_PATH="C:/Qt/6.6.0/msvc2019_64"
```

**If Qt and dependencies are system-installed:**
```powershell
cmake .. -DCMAKE_PREFIX_PATH="C:/Qt/6.6.0/msvc2019_64"
```

**Expected output:**
```
-- The CXX compiler identification is MSVC 19.x.x
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: C:/Program Files/Microsoft Visual Studio/...
-- Found Qt6: ...
-- Found CURL: ...
-- Found SQLite3: ...
-- Configuring done
-- Generating done
-- Build files have been written to: .../build
```

### Step 4: Build

```powershell
cmake --build . --config Release
```

**This will take 2-5 minutes**

**Expected output:**
```
Scanning dependencies of target DockerHomelabManager
[ 10%] Building CXX object CMakeFiles/DockerHomelabManager.dir/src/main.cpp.obj
[ 20%] Building CXX object CMakeFiles/DockerHomelabManager.dir/src/docker/DockerClient.cpp.obj
...
[100%] Built target DockerHomelabManager
```

### Step 5: Run the Application

```powershell
.\Release\DockerHomelabManager.exe
```

**OR from project root:**
```powershell
cd ..
.\build\Release\DockerHomelabManager.exe
```

---

## Troubleshooting

### Error: "CMake not found"
**Solution:** Add CMake to PATH
```powershell
# Add to PATH temporarily
$env:Path += ";C:\Program Files\CMake\bin"

# Or permanently via System Properties > Environment Variables
```

### Error: "Qt6 not found"
**Solution:** Specify Qt path explicitly
```powershell
cmake .. -DCMAKE_PREFIX_PATH="C:/Qt/6.6.0/msvc2019_64" -G "Visual Studio 17 2022" -A x64
```

### Error: "CURL not found" or "SQLite3 not found"
**Solution:** Install via vcpkg and specify toolchain
```powershell
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### Error: "No CMAKE_CXX_COMPILER could be found"
**Solution:** Install Visual Studio Build Tools
```powershell
winget install Microsoft.VisualStudio.2022.BuildTools
# Then restart PowerShell and try again
```

### Error: "nlohmann/json download failed"
**Solution:** Download manually or check internet connection
```powershell
# CMake will auto-download via FetchContent
# If it fails, check firewall/proxy settings
```

### Application won't start: "MSVCP140.dll missing"
**Solution:** Install Visual C++ Redistributable
```powershell
winget install Microsoft.VCRedist.2015+.x64
```

### Application won't start: Qt DLLs missing
**Solution:** Copy Qt DLLs to Release folder
```powershell
# From build/Release folder:
copy "C:\Qt\6.6.0\msvc2019_64\bin\Qt6Core.dll" .
copy "C:\Qt\6.6.0\msvc2019_64\bin\Qt6Gui.dll" .
copy "C:\Qt\6.6.0\msvc2019_64\bin\Qt6Widgets.dll" .
copy "C:\Qt\6.6.0\msvc2019_64\bin\Qt6Network.dll" .

# Or use windeployqt (better):
C:\Qt\6.6.0\msvc2019_64\bin\windeployqt.exe DockerHomelabManager.exe
```

---

## Quick Build Script (PowerShell)

Save this as `build.ps1`:

```powershell
# Docker Homelab Manager Build Script

Write-Host "Building Docker Homelab Manager..." -ForegroundColor Green

# Check CMake
if (!(Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: CMake not found. Installing..." -ForegroundColor Red
    winget install Kitware.CMake
    Write-Host "Please restart PowerShell and run this script again." -ForegroundColor Yellow
    exit 1
}

# Set Qt path (adjust as needed)
$QT_PATH = "C:\Qt\6.6.0\msvc2019_64"
if (!(Test-Path $QT_PATH)) {
    Write-Host "WARNING: Qt not found at $QT_PATH" -ForegroundColor Yellow
    $QT_PATH = Read-Host "Enter Qt installation path"
}

# Create build directory
if (Test-Path build) {
    Write-Host "Cleaning old build..." -ForegroundColor Yellow
    Remove-Item build -Recurse -Force
}
New-Item -ItemType Directory -Path build | Out-Null

# Configure
Write-Host "Configuring with CMake..." -ForegroundColor Cyan
Push-Location build
cmake .. -DCMAKE_PREFIX_PATH="$QT_PATH" -G "Visual Studio 17 2022" -A x64

if ($LASTEXITCODE -ne 0) {
    Write-Host "Configuration failed!" -ForegroundColor Red
    Pop-Location
    exit 1
}

# Build
Write-Host "Building (this may take a few minutes)..." -ForegroundColor Cyan
cmake --build . --config Release

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    Pop-Location
    exit 1
}

# Deploy Qt DLLs
Write-Host "Deploying Qt dependencies..." -ForegroundColor Cyan
& "$QT_PATH\bin\windeployqt.exe" "Release\DockerHomelabManager.exe"

Pop-Location

Write-Host ""
Write-Host "Build successful! " -ForegroundColor Green -NoNewline
Write-Host "Executable: build\Release\DockerHomelabManager.exe" -ForegroundColor White
Write-Host ""
Write-Host "Run with: .\build\Release\DockerHomelabManager.exe" -ForegroundColor Cyan
```

**Run the script:**
```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
```

---

## Alternative: Using Visual Studio

### 1. Open Project in Visual Studio

1. Launch Visual Studio 2022
2. File → Open → CMake...
3. Select `CMakeLists.txt` from project root
4. Visual Studio will auto-configure

### 2. Configure Qt Path

- Right-click `CMakeLists.txt` → CMake Settings
- Add CMake variable: `CMAKE_PREFIX_PATH` = `C:/Qt/6.6.0/msvc2019_64`

### 3. Build

- Build → Build All (Ctrl+Shift+B)
- Or select "DockerHomelabManager.exe" and press F5 to build and run

### 4. Run

- Debug → Start Without Debugging (Ctrl+F5)

---

## Deployment

### Creating a Redistributable Package

```powershell
cd build\Release

# Copy executable
mkdir DockerHomelabManager
copy DockerHomelabManager.exe DockerHomelabManager\

# Deploy Qt DLLs
C:\Qt\6.6.0\msvc2019_64\bin\windeployqt.exe DockerHomelabManager\DockerHomelabManager.exe

# Copy CURL and SQLite DLLs (if not statically linked)
copy C:\vcpkg\installed\x64-windows\bin\*.dll DockerHomelabManager\

# Create ZIP
Compress-Archive -Path DockerHomelabManager -DestinationPath DockerHomelabManager-v0.2.0-Windows.zip
```

---

## Verification

After building, verify the executable:

```powershell
cd build\Release

# Check file exists
dir DockerHomelabManager.exe

# Check dependencies
dumpbin /dependents DockerHomelabManager.exe

# Run
.\DockerHomelabManager.exe
```

**Expected startup:**
1. Window opens with "Docker Homelab Manager v0.2.0"
2. Tabs visible: Dashboard, Containers, Networks, Updates, Diagnostics
3. Toolbar with buttons: Refresh, Start, Stop, Restart, Check Updates
4. Status bar shows "Ready"

---

## Performance

**Build times (approximate):**
- First build: 3-5 minutes
- Incremental build: 30 seconds - 2 minutes
- Clean rebuild: 2-4 minutes

**Executable size:**
- Without Qt DLLs: ~500 KB - 2 MB
- With Qt DLLs: ~20-30 MB
- Full deployment package: ~30-40 MB

---

## Next Steps

After successful build:
1. ✅ Run the application
2. ✅ Test Docker connection
3. ✅ Try starting/stopping containers
4. ✅ Check port scanner
5. ✅ Test update checker

**Enjoy your Docker Homelab Manager!** 🚀
