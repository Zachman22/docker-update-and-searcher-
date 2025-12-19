# Quick Start Guide - Docker Homelab Manager

Get up and running in 10 minutes!

## Prerequisites

**You Need:**
- Docker installed and running
- C++17 compatible compiler
- CMake 3.16+
- Qt6
- libcurl
- SQLite3

## Installation

### Windows (10 minutes)

```powershell
# 1. Install Qt6
winget install Qt.Qt

# 2. Install vcpkg (if not already installed)
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# 3. Install dependencies
.\vcpkg install curl:x64-windows sqlite3:x64-windows

# 4. Clone repository
cd C:\Users\YourName\Documents
git clone https://github.com/Zachman22/docker-update-and-searcher-.git
cd docker-update-and-searcher-

# 5. Build
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release

# 6. Run
.\Release\DockerHomelabManager.exe
```

### Linux (5 minutes)

```bash
# 1. Install dependencies
sudo apt-get update
sudo apt-get install -y cmake build-essential qt6-base-dev \
    libcurl4-openssl-dev libsqlite3-dev git

# 2. Clone repository
cd ~/Projects
git clone https://github.com/Zachman22/docker-update-and-searcher-.git
cd docker-update-and-searcher-

# 3. Build
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)

# 4. Run
./DockerHomelabManager
```

### macOS (5 minutes)

```bash
# 1. Install dependencies
brew install cmake qt@6 curl sqlite git

# 2. Clone repository
cd ~/Projects
git clone https://github.com/Zachman22/docker-update-and-searcher-.git
cd docker-update-and-searcher-

# 3. Build
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)
cmake --build . -j$(sysctl -n hw.ncpu)

# 4. Run
./DockerHomelabManager
```

## First Run

When you first launch the application:

1. **Connection Check**: The app will attempt to connect to Docker
   - Linux/macOS: Uses `/var/run/docker.sock`
   - Windows: Uses `//./pipe/docker_engine`

2. **Container Discovery**: All containers will be scanned and displayed

3. **Port Scan**: System ports will be scanned for conflicts

4. **Database Creation**: A local SQLite database will be created

## Basic Usage

### View Containers

The main window shows all your containers with:
- Name and ID
- Current state (running/stopped)
- Image and tag
- Port mappings
- Last updated time

### Check for Updates

1. Click **"Check Updates"** button
2. Wait for Docker Hub API calls (few seconds)
3. View available updates in the Updates tab
4. See which containers have newer versions

### Detect Port Conflicts

1. Navigate to **Networks** tab
2. Click **"Scan Ports"**
3. View all open ports on your system
4. See conflicts and suggested alternative ports

### Start/Stop Containers

1. Select a container from the list
2. Click **Start**, **Stop**, or **Restart**
3. Container state updates automatically

## Testing Without GUI

Want to test the backend? Run the example:

```bash
cd build
# Compile example
g++ -std=c++17 ../examples/example_usage.cpp \
    -I../include -lsqlite3 -lcurl \
    -o example_test

# Run it
./example_test
```

This will:
- Connect to Docker
- List all containers
- Scan ports
- Check for updates
- Save to database

## Configuration

### Docker Connection

**Linux/macOS**: Default `/var/run/docker.sock`

```bash
# If using custom socket location
export DOCKER_HOST=unix:///custom/path/docker.sock
```

**Windows**: Default named pipe `//./pipe/docker_engine`

If Docker Desktop isn't running on default pipe, expose on TCP:
```powershell
# In Docker Desktop settings, enable "Expose daemon on tcp://localhost:2375"
```

### Database Location

Default: `homelab_manager.db` in current directory

To change:
```cpp
auto database = std::make_shared<storage::Database>("/custom/path/app.db");
```

### Logging

Default: Logs to console and `docker_homelab_manager.log`

To adjust log level:
```cpp
utils::Logger::getInstance().setLogLevel(utils::LogLevel::Debug);
```

## Common Issues

### "Cannot connect to Docker daemon"

**Linux/macOS:**
```bash
# Check if Docker is running
docker ps

# Check socket permissions
ls -l /var/run/docker.sock

# Add user to docker group
sudo usermod -aG docker $USER
# Log out and back in
```

**Windows:**
```powershell
# Make sure Docker Desktop is running
# Check in system tray

# If named pipe doesn't work, enable TCP:
# Docker Desktop → Settings → General → Expose daemon on tcp://localhost:2375
```

### "Qt library not found"

**Linux:**
```bash
# Install Qt6
sudo apt-get install qt6-base-dev

# Or set Qt path
export CMAKE_PREFIX_PATH=/path/to/qt6
```

**Windows:**
```powershell
# Make sure Qt is in PATH
$env:PATH += ";C:\Qt\6.x.x\msvc2019_64\bin"
```

### "Port scanner returns empty"

**Linux:**
```bash
# May need sudo for /proc access
sudo ./DockerHomelabManager
```

**Windows:**
```powershell
# Run as Administrator for full process info
```

### Build Errors

**Missing nlohmann/json:**
- CMake should auto-download via FetchContent
- If it fails, manually download json.hpp to `external/`

**SQLite3 not found:**
```bash
# Linux
sudo apt-get install libsqlite3-dev

# Windows
vcpkg install sqlite3

# macOS
brew install sqlite
```

## What Works Right Now

✅ **Docker Operations**
- List all containers
- Start/stop/restart containers
- Get container details
- Pull images
- Manage networks and volumes

✅ **Port Management**
- Scan all open ports
- Detect conflicts
- Suggest alternatives
- Find available port ranges

✅ **Update Detection**
- Check Docker Hub for updates
- Compare image digests
- List available versions
- Filter by update strategy

✅ **Data Persistence**
- Save container states
- Track operation history
- Log update attempts
- Store issues and resolutions

## What's Coming Soon

⏳ **v0.2.1 (Next Week)**
- Functional GUI with data display
- Click-to-start/stop containers
- Visual port conflict warnings
- Update notifications

⏳ **v0.3.0**
- Container dependency detection
- Safe start/stop operations
- Batch actions

⏳ **v0.4.0**
- Network connectivity tests
- DNS diagnostics
- Auto-fix common errors

## Getting Help

- **Documentation**: See `docs/` directory
- **Examples**: See `examples/` directory
- **Issues**: https://github.com/Zachman22/docker-update-and-searcher-/issues
- **Discussions**: https://github.com/Zachman22/docker-update-and-searcher-/discussions

## Next Steps

1. ✅ Get the app running
2. 📖 Read the [Architecture](docs/ARCHITECTURE.md) doc
3. 🎯 Check the [Roadmap](docs/ROADMAP.md)
4. 🤝 Consider [Contributing](CONTRIBUTING.md)
5. ⭐ Star the repo if you find it useful!

---

**Questions?** Open an issue on GitHub!
**Want to contribute?** PRs welcome!
**Found a bug?** Please report it!

Happy homelabbing! 🏠🐳
