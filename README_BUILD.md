# 🚀 Quick Start - Building Your Executable

**You asked for an .exe file. Here's how to build it!**

## 🎯 The Fastest Way (3 commands)

```powershell
# 1. Install prerequisites (one-time setup)
winget install Kitware.CMake Microsoft.VisualStudio.2022.Community

# 2. Navigate to project
cd "C:\Users\Zach\Documents\docker updater and searcher"

# 3. Run automated build script
powershell -ExecutionPolicy Bypass -File build.ps1
```

**Done!** Your executable is at: `build\Release\DockerHomelabManager.exe`

---

## 📋 What You Need First

Before building, you need these installed:

| Tool | Purpose | Install Command |
|------|---------|----------------|
| **CMake** | Build system | `winget install Kitware.CMake` |
| **Visual Studio** | C++ compiler | `winget install Microsoft.VisualStudio.2022.Community` |
| **Qt6** | GUI framework | Download from [qt.io](https://www.qt.io/download) |
| **vcpkg** | Package manager | See instructions below |

### Installing vcpkg (for CURL & SQLite3)

```powershell
# Clone vcpkg
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg

# Bootstrap
.\bootstrap-vcpkg.bat

# Install dependencies
.\vcpkg install curl:x64-windows sqlite3:x64-windows

# Integrate with Visual Studio
.\vcpkg integrate install
```

**Time required**: 15-30 minutes (one-time setup)

---

## 🔨 Build Methods

### Option 1: Automated Script (Recommended) ⭐

**Easiest and fastest!**

```powershell
cd "C:\Users\Zach\Documents\docker updater and searcher"
powershell -ExecutionPolicy Bypass -File build.ps1
```

The script will:
- ✅ Check all prerequisites
- ✅ Configure CMake automatically
- ✅ Build Release version
- ✅ Deploy Qt DLLs
- ✅ Show you where the .exe is

**Takes**: 3-5 minutes

---

### Option 2: Manual Build

```powershell
cd "C:\Users\Zach\Documents\docker updater and searcher"
mkdir build
cd build

# Configure (adjust Qt path to match your installation)
cmake .. -DCMAKE_PREFIX_PATH="C:/Qt/6.6.0/msvc2019_64" -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" -G "Visual Studio 17 2022" -A x64

# Build
cmake --build . --config Release

# Deploy Qt DLLs
C:\Qt\6.6.0\msvc2019_64\bin\windeployqt.exe Release\DockerHomelabManager.exe
```

---

### Option 3: Visual Studio GUI

1. Open Visual Studio 2022
2. **File** → **Open** → **CMake...**
3. Select `CMakeLists.txt`
4. Right-click `CMakeLists.txt` → **CMake Settings**
5. Add variable: `CMAKE_PREFIX_PATH` = `C:/Qt/6.6.0/msvc2019_64`
6. **Build** → **Build All** (Ctrl+Shift+B)

---

## 🎮 Running Your Application

After building:

```powershell
# From project root
.\build\Release\DockerHomelabManager.exe

# Or navigate there
cd build\Release
.\DockerHomelabManager.exe
```

**Important**: Make sure Docker Desktop is running first!

---

## ✅ What to Expect

When you run the .exe:

1. **Window opens** - "Docker Homelab Manager v0.2.0"
2. **Five tabs appear**:
   - Dashboard (stats overview)
   - Containers (manage your containers)
   - Networks (port scanning)
   - Updates (check for updates)
   - Diagnostics (troubleshoot issues)
3. **Toolbar buttons**:
   - Refresh - Update container list
   - Start/Stop/Restart - Manage containers
   - Check Updates - Find available updates
   - Run Diagnostics - Scan for issues

---

## 🆘 Troubleshooting

### "CMake not found"
```powershell
winget install Kitware.CMake
# Restart PowerShell
```

### "Qt6 not found"
- Download from: https://www.qt.io/download-qt-installer
- Install "Qt 6.x.x for MSVC 2019/2022 64-bit"
- Note the installation path

### "CURL or SQLite3 not found"
```powershell
cd C:\vcpkg
.\vcpkg install curl:x64-windows sqlite3:x64-windows
.\vcpkg integrate install
```

### "DLL missing" when running
```powershell
cd build\Release
C:\Qt\6.6.0\msvc2019_64\bin\windeployqt.exe DockerHomelabManager.exe
```

### Build fails
1. Check you have Visual Studio 2022 (not 2019)
2. Make sure Qt path is correct
3. Try clean build: delete `build` folder and rebuild

---

## 📂 File Locations

After successful build:

```
project-root/
├── build/
│   └── Release/
│       ├── DockerHomelabManager.exe  ← Your executable!
│       ├── Qt6Core.dll                (auto-deployed)
│       ├── Qt6Gui.dll                 (auto-deployed)
│       ├── Qt6Widgets.dll             (auto-deployed)
│       └── ... (other DLLs)
```

---

## 📦 Creating a Redistributable Package

Want to share with others?

```powershell
cd build\Release

# Create folder
mkdir DockerHomelabManager-Portable

# Copy files
copy *.exe DockerHomelabManager-Portable\
copy *.dll DockerHomelabManager-Portable\

# Create ZIP
Compress-Archive -Path DockerHomelabManager-Portable -DestinationPath DockerHomelabManager-v0.2.0-Windows.zip
```

Now you can share `DockerHomelabManager-v0.2.0-Windows.zip`!

---

## 🎯 Quick Reference

| Command | What it does |
|---------|-------------|
| `build.ps1` | Automated build (easiest) |
| `cmake --build . --config Release` | Rebuild after changes |
| `.\build\Release\DockerHomelabManager.exe` | Run the app |
| `docker ps` | Verify Docker is running |

---

## 📚 More Information

- **Detailed Instructions**: See `BUILD_INSTRUCTIONS.md`
- **Installation Checklist**: See `INSTALL_CHECKLIST.md`
- **Troubleshooting**: See `BUILD_INSTRUCTIONS.md` → Troubleshooting section

---

## 💪 Your Application Features

Once built, your .exe can:

✅ **Manage Docker Containers**
- List all containers with details
- Start, stop, restart with one click
- Color-coded status (green=running, red=stopped)

✅ **Detect Port Conflicts**
- Scan all open ports on your system
- Identify which process uses each port
- Suggest alternative ports
- Prevent conflicts before they happen

✅ **Check for Updates**
- Query Docker Hub for latest versions
- Compare image digests (not just tags)
- Show which containers have updates
- Batch update checking

✅ **Persistent Storage**
- SQLite database tracks everything
- Container history and actions
- Update history with success/failure
- Never lose your data

✅ **Professional GUI**
- Beautiful Qt6 interface
- Intuitive tabs and buttons
- Status bar feedback
- Confirmation dialogs

---

## 🎉 You're Done!

Your Docker Homelab Manager is ready to build. Follow the steps above, and in about 20-30 minutes (including setup time), you'll have a fully functional Docker management application!

**Questions?** Check the other documentation files or open a GitHub issue.

**Happy building!** 🚀
