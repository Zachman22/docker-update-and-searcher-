# Docker Homelab Manager

A powerful C++17 desktop application for managing Docker containers in homelab environments with intelligent network troubleshooting, automatic updates, and dependency resolution.

## Features

### Core Functionality
- **Container Discovery & Management** - View, start, stop, and manage all Docker containers
- **Intelligent Update System** - Check for updates and safely update containers with automatic rollback
- **Dependency Resolution** - Automatically detect and pull container dependencies
- **Network Troubleshooting** - Comprehensive network diagnostics and error detection

### Network Diagnostics
- **Port Conflict Detection** - Identify and resolve port conflicts before they cause issues
- **Port Mapping Visualization** - See all open ports and their assignments
- **Network Connectivity Testing** - Verify container-to-container and external connectivity
- **DNS Resolution Checks** - Diagnose DNS issues in Docker networks

### Error Diagnostics
- **Common Error Detection** - Automatically identify common Docker issues
- **Fix Suggestions** - Get actionable recommendations to resolve problems
- **Permission Issues** - Detect and guide through permission problems
- **Resource Monitoring** - Track container resource usage and identify bottlenecks

### Homelab-Focused Features
- **Registry Authentication** - Support for Docker Hub, GitHub Container Registry, and more
- **Selective Updates** - Choose which containers to update with dependency awareness
- **Safe Restart Logic** - Automatically restart containers or Docker daemon when needed
- **Local Storage** - SQLite-based configuration and history tracking

### 🤖 AI Automation Features (NEW!)

This project includes an advanced AI-powered automation system that accelerates development:

- **AI Feature Generator** - Uses Claude AI to automatically analyze the codebase and implement new features
- **Automated TODO Management** - Scans code for TODOs and maintains organized task lists
- **Continuous Integration** - GitHub Actions automatically builds executables for Windows, Linux, and macOS
- **Cross-Platform Builds** - Automated packaging (EXE, AppImage, DMG) with artifact uploads
- **Smart Prioritization** - AI determines optimal feature implementation order based on dependencies and impact

**Benefits:**
- 🚀 Faster development with AI-assisted coding
- 📋 Always up-to-date task tracking
- 🔨 Automated multi-platform builds
- 📦 Ready-to-use executables for all platforms
- 🔄 Continuous feature delivery

See [`ai_automation/README.md`](ai_automation/README.md) for detailed documentation on using the AI automation system.

## Technology Stack

- **Language**: C++17
- **GUI Framework**: Qt6 (elegant, cross-platform interface)
- **Storage**: SQLite3 (local database)
- **Docker API**: REST API via libcurl
- **JSON Parsing**: nlohmann/json
- **Networking**: Qt Network + native socket APIs

## Building from Source

### Prerequisites

#### Windows
- Visual Studio 2019 or newer (with C++17 support)
- CMake 3.16+
- Qt6 (https://www.qt.io/download)
- vcpkg for dependencies (recommended)

#### Linux
```bash
# Ubuntu/Debian
sudo apt-get install cmake build-essential qt6-base-dev libcurl4-openssl-dev libsqlite3-dev

# Fedora/RHEL
sudo dnf install cmake gcc-c++ qt6-qtbase-devel libcurl-devel sqlite-devel
```

#### macOS
```bash
brew install cmake qt@6 curl sqlite
```

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/Zachman22/docker-update-and-searcher-.git
cd docker-update-and-searcher-

# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
cmake --build . --config Release

# Run
./DockerHomelabManager  # Linux/macOS
.\Release\DockerHomelabManager.exe  # Windows
```

## Using AI Automation

The project includes powerful AI automation tools for development. See full documentation in [`ai_automation/README.md`](ai_automation/README.md).

### Quick Start

```bash
# Install Python dependencies
pip install -r ai_automation/requirements.txt

# Set up your Anthropic API key (optional, for AI features)
export ANTHROPIC_API_KEY="your-key-here"

# Generate TODO list from codebase
python ai_automation/todo_manager.py --update

# Run automated build
python ai_automation/build_automation.py

# AI-powered feature generation (requires API key)
python ai_automation/feature_generator.py --max-features 5
```

### Continuous Integration

The project includes GitHub Actions workflows that automatically:
- Build executables for Windows, Linux, and macOS
- Package applications (ZIP, AppImage, DMG)
- Run tests and quality checks
- Create GitHub releases with downloadable binaries
- Update TODO list with build status

Workflows run automatically on push to `main` or `claude/*` branches.

### Available Automation Tools

| Tool | Description | Usage |
|------|-------------|-------|
| **feature_generator.py** | AI-powered feature implementation | `--max-features 10` |
| **todo_manager.py** | TODO scanning and prioritization | `--update --stats` |
| **build_automation.py** | Cross-platform build system | `--no-clean --debug` |

## Configuration

The application stores its configuration in a local SQLite database. On first run, it will:
1. Detect your Docker installation
2. Scan existing containers
3. Create a local database for settings and history

## Usage

### First Run
1. Launch the application
2. Grant Docker socket access if prompted
3. The dashboard will populate with your current containers

### Checking for Updates
1. Click "Check All Updates" to scan all containers
2. Review available updates with dependency information
3. Select containers to update
4. Click "Update Selected" for safe, automatic updates

### Network Diagnostics
1. Navigate to the "Networks" tab
2. View port mappings and conflicts
3. Run connectivity tests
4. Get fix suggestions for detected issues

### Troubleshooting Errors
1. Issues appear automatically in the dashboard
2. Click on any issue for detailed diagnostics
3. Follow suggested fixes
4. Apply fixes automatically when available

## Roadmap

### Version 0.1.0 (MVP) - Current
- [x] Project structure and build system
- [ ] Docker API integration
- [ ] Container listing and basic management
- [ ] Port conflict detection
- [ ] Update checking (Docker Hub)
- [ ] Basic GUI implementation
- [ ] SQLite storage layer

### Version 0.2.0
- [ ] Network connectivity diagnostics
- [ ] Dependency resolution engine
- [ ] Error diagnosis system with auto-fix
- [ ] Registry authentication
- [ ] Safe update process with rollback

### Version 0.3.0
- [ ] Docker Compose awareness
- [ ] Update scheduling
- [ ] Health monitoring
- [ ] Backup/restore integration
- [ ] Template library for common stacks

## Contributing

Contributions are welcome! This project is focused on homelab users who want better Docker management tools.

### Areas for Contribution
- Platform-specific optimizations
- Additional registry support
- Error diagnosis patterns
- UI/UX improvements
- Documentation

## License

MIT License - See LICENSE file for details

## Acknowledgments

Built for the homelab community, by the homelab community.

## Support

- Issues: https://github.com/Zachman22/docker-update-and-searcher-/issues
- Discussions: https://github.com/Zachman22/docker-update-and-searcher-/discussions

## Screenshots

_(Coming soon - application is in active development)_

## Architecture

```
┌─────────────────────────────────────────┐
│           Qt6 GUI Layer                 │
│  (MainWindow, Dialogs, Visualizations)  │
└─────────────────┬───────────────────────┘
                  │
┌─────────────────▼───────────────────────┐
│        Business Logic Layer             │
│  ┌──────────────┐  ┌─────────────────┐ │
│  │Container Mgr │  │ Update Checker  │ │
│  ├──────────────┤  ├─────────────────┤ │
│  │Port Scanner  │  │ Error Diagnostics│ │
│  ├──────────────┤  ├─────────────────┤ │
│  │Network Diag  │  │ Dependency Mgr  │ │
│  └──────────────┘  └─────────────────┘ │
└─────────────────┬───────────────────────┘
                  │
┌─────────────────▼───────────────────────┐
│          Data Access Layer              │
│  ┌──────────────┐  ┌─────────────────┐ │
│  │Docker Client │  │SQLite Database  │ │
│  │  (REST API)  │  │   (Config/Log)  │ │
│  └──────────────┘  └─────────────────┘ │
└─────────────────────────────────────────┘
```
