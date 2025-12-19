# Installation Checklist - Docker Homelab Manager

Follow this checklist to build and run the application.

## ☑️ Prerequisites (Install these first)

### 1. ✅ CMake
- [ ] **Download & Install**: `winget install Kitware.CMake`
- [ ] **Verify**: Open new PowerShell and run `cmake --version`
- [ ] **Expected**: `cmake version 3.16` or higher

### 2. ✅ Visual Studio Build Tools
- [ ] **Option A**: `winget install Microsoft.VisualStudio.2022.Community`
- [ ] **Option B**: `winget install Microsoft.VisualStudio.2022.BuildTools`
- [ ] **During install, select**: "Desktop development with C++"
- [ ] **Verify**: Check for `C:\Program Files\Microsoft Visual Studio\2022\`

### 3. ✅ Qt6
- [ ] **Download**: https://www.qt.io/download-qt-installer
- [ ] **Install**: Qt 6.x.x for MSVC 2019/2022 64-bit
- [ ] **Note path**: `C:\Qt\6.6.0\msvc2019_64` (example)
- [ ] **Verify**: Check folder exists

### 4. ✅ CURL & SQLite3 (via vcpkg)
```powershell
# Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat

# Install dependencies
.\vcpkg install curl:x64-windows sqlite3:x64-windows
.\vcpkg integrate install
```
- [ ] **Verify**: Check `C:\vcpkg\installed\x64-windows\`

---

## 🔨 Building (Choose one method)

### Method 1: Automated Build Script (Easiest) ⭐

1. Open PowerShell **as Administrator**
2. Navigate to project:
   ```powershell
   cd "C:\Users\Zach\Documents\docker updater and searcher"
   ```
3. Run build script:
   ```powershell
   powershell -ExecutionPolicy Bypass -File build.ps1
   ```
4. Follow prompts (it will ask for Qt path if needed)

**That's it!** Skip to "Running the Application" below.

---

### Method 2: Manual Build (More Control)

1. Open PowerShell **as Administrator**

2. Navigate to project:
   ```powershell
   cd "C:\Users\Zach\Documents\docker updater and searcher"
   ```

3. Create build directory:
   ```powershell
   mkdir build
   cd build
   ```

4. Configure (adjust Qt path):
   ```powershell
   cmake .. -DCMAKE_PREFIX_PATH="C:/Qt/6.6.0/msvc2019_64" -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" -G "Visual Studio 17 2022" -A x64
   ```

5. Build:
   ```powershell
   cmake --build . --config Release
   ```

6. Deploy Qt DLLs:
   ```powershell
   C:\Qt\6.6.0\msvc2019_64\bin\windeployqt.exe Release\DockerHomelabManager.exe
   ```

---

### Method 3: Visual Studio (GUI)

1. Open Visual Studio 2022
2. File → Open → CMake...
3. Select `CMakeLists.txt` from project root
4. Right-click CMakeLists.txt → CMake Settings
5. Add: `CMAKE_PREFIX_PATH` = `C:/Qt/6.6.0/msvc2019_64`
6. Build → Build All (Ctrl+Shift+B)
7. Debug → Start Without Debugging (Ctrl+F5)

---

## 🚀 Running the Application

### First Time Setup

1. **Make sure Docker Desktop is running!**
   - Open Docker Desktop
   - Wait for green "Docker is running" indicator

2. **Navigate to executable**:
   ```powershell
   cd "C:\Users\Zach\Documents\docker updater and searcher\build\Release"
   ```

3. **Run**:
   ```powershell
   .\DockerHomelabManager.exe
   ```

### Expected Behavior

- ✅ Window opens with title "Docker Homelab Manager v0.2.0"
- ✅ Five tabs visible: Dashboard, Containers, Networks, Updates, Diagnostics
- ✅ Toolbar with buttons: Refresh, Start, Stop, Restart, etc.
- ✅ Status bar shows "Ready" or "Refreshing containers..."
- ✅ Containers tab shows your Docker containers (if any)

---

## 🧪 Quick Test

Once the app is running:

1. **Go to Containers tab**
2. **Check if your containers are listed**
   - You should see container names, images, states
   - Running containers in green, stopped in red
3. **Select a stopped container**
4. **Click "Start" button**
5. **Verify it starts** (watch for success message)
6. **Click "Stop" button** to stop it again

---

## ❌ Troubleshooting

### "CMake not found"
```powershell
winget install Kitware.CMake
# Then restart PowerShell
```

### "Qt6 not found"
- Check Qt path is correct in build command
- Make sure you installed Qt6 (not Qt5)

### "CURL not found" or "SQLite3 not found"
```powershell
cd C:\vcpkg
.\vcpkg install curl:x64-windows sqlite3:x64-windows
.\vcpkg integrate install
```

### "DLL not found" when running
```powershell
# Run windeployqt again
cd build\Release
C:\Qt\6.6.0\msvc2019_64\bin\windeployqt.exe DockerHomelabManager.exe
```

### Application crashes on startup
1. Check Docker Desktop is running
2. Check you have Docker containers (create one if needed)
3. Run from PowerShell to see error messages

### "Can't connect to Docker"
- Make sure Docker Desktop is running
- Try: `docker ps` in PowerShell to verify Docker works
- Restart Docker Desktop if needed

---

## 📋 Installation Summary

**Time required**:
- Installing prerequisites: 15-30 minutes (one-time)
- Building application: 3-5 minutes
- Total first-time setup: 20-35 minutes

**Disk space required**:
- Visual Studio: ~7 GB
- Qt6: ~3 GB
- vcpkg + dependencies: ~1 GB
- Project build: ~500 MB
- **Total**: ~11-12 GB

**After installation, you have**:
- ✅ Fully functional Docker management GUI
- ✅ Port conflict detection
- ✅ Update checker
- ✅ Container start/stop/restart
- ✅ SQLite database for history

---

## 🎯 Quick Reference

**Build from scratch**:
```powershell
cd "C:\Users\Zach\Documents\docker updater and searcher"
powershell -ExecutionPolicy Bypass -File build.ps1
```

**Run application**:
```powershell
.\build\Release\DockerHomelabManager.exe
```

**Rebuild after code changes**:
```powershell
cd build
cmake --build . --config Release
```

**Clean rebuild**:
```powershell
Remove-Item build -Recurse -Force
powershell -ExecutionPolicy Bypass -File build.ps1
```

---

## ✅ Success Criteria

You know it's working when:
- [x] Application window opens
- [x] No crash on startup
- [x] Containers tab shows your Docker containers
- [x] You can start/stop containers with buttons
- [x] Status bar updates with actions
- [x] Log file created: `docker_homelab_manager.log`

**Congratulations! Your Docker Homelab Manager is ready to use!** 🎉

---

## 📞 Need Help?

1. **Check BUILD_INSTRUCTIONS.md** for detailed troubleshooting
2. **Check logs**: Look at `docker_homelab_manager.log` in app directory
3. **GitHub Issues**: https://github.com/Zachman22/docker-update-and-searcher-/issues
