# AI Automation System

This directory contains the AI-powered automation system for Docker Homelab Manager. The system uses Claude AI to continuously generate features, manage tasks, build executables, and maintain GitHub integration.

## Features

### 🤖 AI Feature Generator (`feature_generator.py`)

Continuously analyzes the codebase and generates new features using Claude AI.

**Capabilities:**
- Analyzes architecture and roadmap to determine next best features
- Generates complete C++ implementations
- Automatically commits and pushes to GitHub
- Maintains feature generation logs

**Usage:**
```bash
# Run continuous feature generation (10 features)
python ai_automation/feature_generator.py --max-features 10

# Generate single feature
python ai_automation/feature_generator.py --single

# Generate without auto-commit
python ai_automation/feature_generator.py --no-commit
```

**Environment:**
```bash
export ANTHROPIC_API_KEY="your-api-key-here"
```

### 📋 TODO Manager (`todo_manager.py`)

Scans the codebase for TODO comments and maintains an organized TODO.md file.

**Capabilities:**
- Scans all C++ files for TODO comments
- Categorizes tasks by component
- Uses AI to prioritize tasks
- Generates implementation phase recommendations
- Calculates project completion statistics

**Usage:**
```bash
# Update TODO.md from codebase
python ai_automation/todo_manager.py --update

# Show completion statistics
python ai_automation/todo_manager.py --stats
```

### 🔨 Build Automation (`build_automation.py`)

Cross-platform build automation for creating executables.

**Capabilities:**
- Supports Windows, Linux, and macOS
- Automatic dependency detection
- CMake configuration and building
- Platform-specific packaging (ZIP, AppImage, DMG)
- Test execution

**Usage:**
```bash
# Full build and package
python ai_automation/build_automation.py

# Debug build
python ai_automation/build_automation.py --debug

# Don't clean before building
python ai_automation/build_automation.py --no-clean

# Specify parallel jobs
python ai_automation/build_automation.py --jobs 8
```

### ⚙️ GitHub Actions CI/CD

Automated workflows for continuous integration and deployment.

**Workflows:**

1. **AI Continuous Integration** (`.github/workflows/ai-continuous-integration.yml`)
   - Triggered on push to main or claude/* branches
   - Builds on Linux, Windows, and macOS
   - Creates executables and packages
   - Uploads artifacts
   - Creates GitHub releases
   - Updates TODO.md with build status

**Features:**
- Multi-platform builds (Ubuntu, Windows, macOS)
- Automated executable packaging
- GitHub Release creation with artifacts
- Optional AI feature generation
- Build status tracking

**Manual Workflow Dispatch:**
```bash
# Trigger build with release creation
gh workflow run ai-continuous-integration.yml -f deploy_release=true
```

## Installation

### Prerequisites

1. **Python 3.11+**
2. **Anthropic API Key** (for AI features)
3. **Build tools** (per platform):
   - **Linux**: `cmake`, `build-essential`, `qt6-base-dev`, `libsqlite3-dev`, `libcurl4-openssl-dev`
   - **Windows**: Visual Studio 2022, CMake, Qt6, vcpkg
   - **macOS**: Xcode, Homebrew, `brew install cmake qt@6 sqlite curl`

### Setup

```bash
# Install Python dependencies
pip install -r ai_automation/requirements.txt

# Set up API key
export ANTHROPIC_API_KEY="sk-ant-..."

# Or use .env file
echo "ANTHROPIC_API_KEY=sk-ant-..." > .env
```

### GitHub Secrets

For CI/CD workflows, add these secrets to your GitHub repository:

```
Settings → Secrets and variables → Actions → New repository secret
```

**Required:**
- `ANTHROPIC_API_KEY`: Your Anthropic API key (optional, only for AI feature generation)

## Workflow Examples

### Continuous Development Workflow

```bash
# 1. Generate TODO list from codebase
python ai_automation/todo_manager.py --update

# 2. Generate and implement features automatically
python ai_automation/feature_generator.py --max-features 5

# 3. Build and package
python ai_automation/build_automation.py

# 4. Features are auto-committed and pushed by feature_generator
# GitHub Actions will automatically build on all platforms
```

### Manual Development Workflow

```bash
# 1. Check what needs to be done
python ai_automation/todo_manager.py --stats

# 2. Generate single feature without commit
python ai_automation/feature_generator.py --single --no-commit

# 3. Review changes, make manual edits

# 4. Build locally
python ai_automation/build_automation.py

# 5. Commit manually
git add .
git commit -m "Implement feature X"
git push
```

### Release Workflow

```bash
# 1. Ensure all features are implemented
python ai_automation/todo_manager.py --stats

# 2. Build on all platforms (via GitHub Actions)
git tag v0.2.0
git push origin v0.2.0

# GitHub Actions will:
# - Build for Linux, Windows, macOS
# - Create packages
# - Create GitHub Release with binaries
```

## File Structure

```
ai_automation/
├── feature_generator.py    # AI-powered feature generation
├── todo_manager.py          # TODO list management
├── build_automation.py      # Build and packaging
├── requirements.txt         # Python dependencies
├── README.md               # This file
├── features_log.json       # Feature generation log (auto-generated)
└── .env                    # API keys (not in git)
```

## Configuration

### Feature Generator Configuration

Edit `feature_generator.py` to customize:
- `model`: AI model to use (default: claude-sonnet-4-5-20250929)
- `max_tokens`: Maximum tokens per request
- Priority criteria in `get_next_feature_to_implement()`

### Build Configuration

Build settings are in `CMakeLists.txt`:
- C++ standard (C++17)
- Qt6 components
- Dependencies
- Compiler flags

### GitHub Actions Configuration

Edit `.github/workflows/ai-continuous-integration.yml`:
- Trigger conditions
- Build matrix
- Deployment settings
- Artifact retention

## AI Model Selection

The system uses **Claude Sonnet 4.5** for optimal balance of:
- Code generation quality
- Reasoning capability
- Cost efficiency
- Speed

For different needs:
- **Quick prototyping**: Use `claude-haiku` (faster, cheaper)
- **Complex features**: Use `claude-opus` (highest quality)

Change in `feature_generator.py`:
```python
self.model = "claude-opus-4-5-20251101"  # For best quality
```

## Monitoring and Logs

### Feature Generation Logs

```bash
# View feature generation history
cat ai_automation/features_log.json

# Format:
{
  "timestamp": "2025-12-19T10:30:00",
  "feature": {
    "feature_name": "Docker API Authentication",
    "component": "DockerClient",
    "priority": "high"
  },
  "status": "completed",
  "code_generated": true
}
```

### Build Logs

Build logs are stored in:
- Local: `build/` directory
- CI/CD: GitHub Actions logs (Actions tab)

### TODO Tracking

- **TODO.md**: Auto-generated task list
- **Codebase**: TODO comments in source files
- Statistics available via `--stats` flag

## Best Practices

### For AI Feature Generation

1. **Review generated code** before deploying
2. **Start with small batches** (--max-features 3)
3. **Use --no-commit** for review-first workflow
4. **Check features_log.json** for history
5. **Keep ARCHITECTURE.md updated** for better AI decisions

### For TODO Management

1. **Add TODO comments** in code as you develop
2. **Run --update regularly** to keep TODO.md current
3. **Use categories** in TODO comments for better organization
4. **Check --stats** to track progress

### For Building

1. **Test locally** before CI/CD
2. **Use --debug** for development
3. **Clean build** for releases
4. **Check dependencies** on new systems

## Troubleshooting

### AI Generation Fails

```bash
# Check API key
echo $ANTHROPIC_API_KEY

# Verify API key works
python -c "import anthropic; client = anthropic.Anthropic(); print('OK')"

# Check logs
cat ai_automation/features_log.json
```

### Build Fails

```bash
# Check dependencies
cmake --version
qt6-qmake --version  # Linux/macOS
qmake6 --version     # Windows

# Clean and rebuild
python ai_automation/build_automation.py --clean

# Check CMake output
cd build && cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON
```

### GitHub Actions Fails

1. Check Actions tab for detailed logs
2. Verify secrets are set (ANTHROPIC_API_KEY)
3. Check build matrix compatibility
4. Review workflow file syntax

## Contributing

When adding automation features:

1. Follow Python PEP 8 style
2. Add comprehensive docstrings
3. Update this README
4. Test on multiple platforms
5. Add error handling

## License

This automation system is part of Docker Homelab Manager and uses the same MIT license.

## Support

- **Issues**: GitHub Issues
- **Discussions**: GitHub Discussions
- **Documentation**: `/docs` directory

---

**Note**: The AI automation system requires an Anthropic API key. Features can still be developed manually without AI assistance.
