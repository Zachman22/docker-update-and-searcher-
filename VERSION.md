# Version History - Docker Homelab Manager

## v0.2.0-alpha (2025-12-18) - Core Backend Complete

**Status**: Production-Ready Backend, GUI Framework Only

### Major Features Completed

#### Docker API Integration ✅
- Full REST API client with libcurl
- Cross-platform socket support (Unix socket + Windows named pipe)
- Complete container lifecycle management
- Image, network, and volume operations
- Registry authentication
- 466 lines of production code

#### Port Conflict Detection ✅
- Platform-specific implementations (Windows iphlpapi, Linux /proc)
- Process name resolution
- Conflict detection before container start
- Alternative port suggestions
- 440 lines of production code

#### Update Checker ✅
- Docker Hub REST API integration
- Tag and digest fetching
- Digest-based update comparison
- Update strategies (conservative, moderate, aggressive)
- 270 lines of production code

#### SQLite Database ✅
- 5-table schema (containers, history, updates, issues, settings)
- Full CRUD operations
- Audit trail and history tracking
- Statistics and cleanup utilities
- 462 lines of production code

### What Works
- ✅ Connect to Docker daemon (all platforms)
- ✅ List, start, stop, restart containers
- ✅ Scan ports and detect conflicts
- ✅ Check for updates from Docker Hub
- ✅ Persistent storage with audit trail
- ✅ Comprehensive logging

### What's Missing
- ⏳ Functional GUI (framework only)
- ⏳ Container dependency detection
- ⏳ Network diagnostics
- ⏳ Error auto-fix

### Statistics
- **Total Lines**: 3,500+
- **Components Complete**: 5 of 8
- **Platforms**: Windows, Linux, macOS
- **Dependencies**: Qt6, libcurl, SQLite3, nlohmann/json

### Breaking Changes
- None (first feature-complete release)

### Known Issues
- Windows named pipe may not work with libcurl (use TCP)
- macOS port scanner basic implementation only
- GUI not yet functional

---

## v0.1.5 (2025-12-18) - Port Scanner & Docker API

### Features
- ✅ Docker API client implementation
- ✅ Port scanner (Windows & Linux)
- ✅ Project infrastructure
- ⚠️ Update checker (stub)
- ⚠️ Database (stub)

### Statistics
- **Total Lines**: 2,800+
- **Components**: 3 of 6

---

## v0.1.0 (2025-12-18) - Initial Release

### Features
- Project structure
- CMake build system
- Architecture documentation
- Header file interfaces
- Stub implementations

### Statistics
- **Total Lines**: 2,810
- **Files**: 25

---

## Upcoming Versions

### v0.2.1 - Functional GUI (Planned)
- QTableWidget for containers
- Dashboard with statistics
- Port conflict warnings
- Update notifications
- Event handlers for all actions

### v0.3.0 - Container Manager (Planned)
- Dependency detection
- Safe start/stop operations
- Startup order calculation
- Batch operations

### v0.4.0 - Advanced Diagnostics (Planned)
- Network connectivity testing
- DNS resolution checks
- Error diagnostics with auto-fix
- Health monitoring dashboard

### v0.5.0 - Update Execution (Planned)
- Safe container updates
- Automatic rollback on failure
- Backup/restore functionality
- Update scheduling

### v1.0.0 - Production Release (Planned)
- Complete GUI
- All features implemented
- Comprehensive testing
- User documentation
- Tutorial videos

---

## Version Numbering

We use Semantic Versioning (MAJOR.MINOR.PATCH):
- **MAJOR**: Breaking changes, major feature sets
- **MINOR**: New features, backward compatible
- **PATCH**: Bug fixes, small improvements
- **-alpha**: Pre-release, testing phase
- **-beta**: Feature complete, stabilization
- **-rc**: Release candidate

---

## Support

- **GitHub**: https://github.com/Zachman22/docker-update-and-searcher-
- **Issues**: Report bugs and request features
- **Discussions**: Ask questions and share ideas

---

**Current Version**: v0.2.0-alpha
**Next Release**: v0.2.1 (GUI Sprint)
**Status**: Backend Complete, GUI In Progress
