# Docker Homelab Manager - Architecture

## Overview

Docker Homelab Manager is designed with a modular, layered architecture to ensure maintainability, testability, and extensibility.

## Layer Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      Presentation Layer                         │
│                        (Qt6 GUI)                                 │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐         │
│  │  MainWindow  │  │   Dialogs    │  │ Visualizations│         │
│  └──────────────┘  └──────────────┘  └──────────────┘         │
└────────────────────────────┬────────────────────────────────────┘
                             │
┌────────────────────────────▼────────────────────────────────────┐
│                     Business Logic Layer                        │
│  ┌──────────────────┐  ┌──────────────────┐                   │
│  │Container Manager │  │ Update Checker   │                   │
│  │- Dependency mgmt │  │- Registry API    │                   │
│  │- Health monitor  │  │- Version compare │                   │
│  └──────────────────┘  └──────────────────┘                   │
│  ┌──────────────────┐  ┌──────────────────┐                   │
│  │  Port Scanner    │  │Error Diagnostics │                   │
│  │- Conflict detect │  │- Auto-fix engine │                   │
│  │- Port suggestion │  │- Issue tracking  │                   │
│  └──────────────────┘  └──────────────────┘                   │
│  ┌──────────────────┐                                          │
│  │Network Diagnostics│                                          │
│  │- Connectivity    │                                          │
│  │- DNS resolution  │                                          │
│  └──────────────────┘                                          │
└────────────────────────────┬────────────────────────────────────┘
                             │
┌────────────────────────────▼────────────────────────────────────┐
│                      Data Access Layer                          │
│  ┌──────────────────┐  ┌──────────────────┐                   │
│  │  Docker Client   │  │ SQLite Database  │                   │
│  │  - REST API      │  │ - Config storage │                   │
│  │  - JSON parsing  │  │ - History logs   │                   │
│  │  - Auth mgmt     │  │ - Issue tracking │                   │
│  └──────────────────┘  └──────────────────┘                   │
└─────────────────────────────────────────────────────────────────┘
```

## Core Components

### 1. Docker Client (`docker/DockerClient`)
**Purpose**: Low-level interface to Docker daemon via REST API

**Responsibilities**:
- HTTP communication with Docker socket/API
- JSON parsing of Docker responses
- Container lifecycle management (start, stop, restart, remove)
- Image operations (pull, list, remove)
- Network and volume management
- Registry authentication

**Key Interfaces**:
- `listContainers()` - Retrieve all containers
- `startContainer(id)` - Start a container
- `pullImage(image, tag)` - Pull from registry
- `login(registry, user, pass)` - Authenticate to registry

### 2. Container Manager (`docker/ContainerManager`)
**Purpose**: High-level container orchestration with dependency awareness

**Responsibilities**:
- Dependency graph construction
- Safe start/stop operations considering dependencies
- Batch operations
- Health monitoring
- Event notification

**Key Interfaces**:
- `analyzeDependencies()` - Build dependency tree
- `safeStartContainer(id)` - Start with dependency resolution
- `getStartupOrder(ids)` - Calculate correct startup sequence

**Dependency Detection Logic**:
1. **Network Dependencies**: Containers on same network
2. **Volume Dependencies**: Shared volume mounts
3. **Link Dependencies**: Docker links (legacy)
4. **Environment Dependencies**: References in env vars

### 3. Port Scanner (`network/PortScanner`)
**Purpose**: Port conflict detection and resolution

**Responsibilities**:
- Scan system for open ports
- Identify process/container using each port
- Detect conflicts before container start
- Suggest alternative ports

**Platform-Specific Implementations**:
- **Windows**: Uses `iphlpapi.h` (GetTcpTable, GetUdpTable)
- **Linux**: Parses `/proc/net/tcp` and `/proc/net/udp`
- **macOS**: Uses `lsof` or system APIs

**Key Algorithms**:
```cpp
// Port conflict detection
for each container to start:
    for each port mapping:
        if port is already bound:
            identify conflicting process
            create PortConflict issue
            suggest next available port
```

### 4. Network Diagnostics (`network/NetworkDiagnostics`)
**Purpose**: Network connectivity testing and troubleshooting

**Responsibilities**:
- Container-to-container connectivity tests
- Internet connectivity verification
- DNS resolution testing
- Network health analysis
- Latency and bandwidth measurement

**Test Types**:
1. **Ping Test**: Basic connectivity
2. **DNS Test**: Name resolution
3. **Port Test**: Specific port connectivity
4. **Network Isolation**: Identify unreachable containers

### 5. Update Checker (`update/UpdateChecker`)
**Purpose**: Container image update management

**Responsibilities**:
- Query registries for latest versions
- Compare current vs available versions
- Manage update strategy (conservative, moderate, aggressive)
- Perform safe updates with rollback
- Backup before update

**Update Strategies**:
- **Conservative**: Only stable releases, manual approval
- **Moderate**: Latest stable, semi-automatic
- **Aggressive**: Latest including pre-releases

**Update Flow**:
```
1. Query registry for image tags
2. Compare digests (not just tags)
3. Check dependencies
4. Create backup/snapshot
5. Stop container
6. Pull new image
7. Recreate container with same config
8. Verify health
9. Rollback if health check fails
```

### 6. Error Diagnostics (`diagnostics/ErrorDiagnostics`)
**Purpose**: Intelligent error detection and auto-fix

**Responsibilities**:
- Pattern-based error detection
- Root cause analysis
- Auto-fix for common issues
- Manual fix suggestions
- Issue categorization and prioritization

**Error Categories**:
- **Network**: Port conflicts, connectivity issues, DNS failures
- **Storage**: Volume mount failures, disk space issues
- **Permission**: Docker socket access, file permissions
- **Resource**: Memory exhaustion, CPU limits
- **Configuration**: Invalid env vars, missing dependencies
- **Registry**: Auth failures, image not found

**Common Auto-Fixes**:
1. Port conflicts → Suggest alternative ports
2. Permission denied → Show chmod/chown commands
3. Volume not found → Create volume automatically
4. Network not found → Create network automatically
5. Image not found → Suggest pull command

### 7. Database (`storage/Database`)
**Purpose**: Persistent storage for configuration and history

**Schema**:
```sql
-- Containers table
CREATE TABLE containers (
    id TEXT PRIMARY KEY,
    name TEXT,
    image TEXT,
    config TEXT,  -- JSON
    created_at DATETIME,
    updated_at DATETIME
);

-- History table
CREATE TABLE container_history (
    id INTEGER PRIMARY KEY,
    container_id TEXT,
    action TEXT,
    timestamp DATETIME,
    details TEXT,
    performed_by TEXT
);

-- Update history
CREATE TABLE update_history (
    id INTEGER PRIMARY KEY,
    container_id TEXT,
    from_version TEXT,
    to_version TEXT,
    timestamp DATETIME,
    success BOOLEAN,
    error_message TEXT,
    rolled_back BOOLEAN
);

-- Issues table
CREATE TABLE issues (
    id TEXT PRIMARY KEY,
    container_id TEXT,
    category TEXT,
    severity TEXT,
    title TEXT,
    description TEXT,
    created_at DATETIME,
    resolved_at DATETIME
);

-- Settings table
CREATE TABLE settings (
    key TEXT PRIMARY KEY,
    value TEXT
);
```

### 8. MainWindow (`ui/MainWindow`)
**Purpose**: Qt6-based user interface

**Views**:
1. **Dashboard**: Overview, statistics, active issues
2. **Containers**: List, manage, view details
3. **Networks**: Port mappings, connectivity tests
4. **Updates**: Available updates, update queue
5. **Diagnostics**: Active issues, health checks

**UI Components**:
- `QTableWidget` for container/issue lists
- `QStatusBar` for real-time feedback
- `QTimer` for auto-refresh
- Custom dialogs for settings, confirmations

## Data Flow Examples

### Container Start Flow
```
User clicks "Start" → MainWindow::onStartContainer()
    ↓
ContainerManager::safeStartContainer(id)
    ↓
1. Check dependencies
2. Get startup order
3. For each dependency: DockerClient::startContainer()
4. Start target container
5. Verify health
6. Record action in Database
    ↓
Update UI with new state
```

### Port Conflict Detection Flow
```
User attempts to start container
    ↓
PortScanner::detectConflictsForContainer(id)
    ↓
1. Get container port mappings
2. For each port: PortScanner::isPortOpen()
3. If open: Get process info
4. Create PortConflict object
    ↓
ErrorDiagnostics::createIssue(conflict)
    ↓
Display in UI with suggested fixes
```

### Update Check Flow
```
Timer triggers OR user clicks "Check Updates"
    ↓
UpdateChecker::checkForUpdates(containers)
    ↓
For each container:
    1. Parse image name and current tag
    2. Query registry API for latest tag
    3. Compare digests
    4. If different: Create UpdateInfo
    ↓
Display in Updates tab
    ↓
User selects updates and clicks "Update"
    ↓
UpdateChecker::performBatchUpdate()
    ↓
For each update (in dependency order):
    1. Database::recordUpdate() (started)
    2. Create backup
    3. Stop container
    4. Pull new image
    5. Start container
    6. Health check
    7. Database::recordUpdate() (completed)
```

## Threading Model

**Main Thread**: UI and event loop
**Worker Threads**: Long-running operations
- Container operations (start, stop, update)
- Network scans
- Update checks
- Diagnostics

**Thread Safety**:
- Use Qt signals/slots for thread communication
- Protect shared data with mutexes
- Use `std::atomic` for flags

## Error Handling Strategy

1. **Graceful Degradation**: If one component fails, others continue
2. **User Notification**: Always inform user of errors
3. **Logging**: Comprehensive logging to file
4. **Recovery**: Automatic retry for transient failures
5. **Rollback**: Always provide rollback for destructive operations

## Future Extensions

### Phase 2 Features
- Docker Compose file parsing and management
- Multi-host Docker management (Docker Swarm/remote)
- Advanced scheduling (cron-based updates)
- Webhook notifications
- Plugin system for custom diagnostics

### Phase 3 Features
- Kubernetes support
- Container resource optimization recommendations
- Cost analysis and optimization
- Backup/restore to cloud storage
- REST API for automation

## Development Guidelines

1. **SOLID Principles**: Each class has single responsibility
2. **Interface Segregation**: Small, focused interfaces
3. **Dependency Injection**: Pass dependencies via constructor
4. **Error Handling**: Use `std::optional` for nullable returns
5. **Modern C++**: Use C++17 features (structured bindings, std::optional, etc.)
6. **Testing**: Unit tests for business logic, integration tests for API calls
