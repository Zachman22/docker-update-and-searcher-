#!/usr/bin/env python3
"""
Build Automation System for Docker Homelab Manager
Automates building, testing, and packaging across platforms
"""

import os
import sys
import subprocess
import platform
import shutil
from pathlib import Path
from datetime import datetime
from typing import Optional, List, Dict

class BuildAutomation:
    """Cross-platform build automation"""

    def __init__(self):
        self.project_root = Path(__file__).parent.parent
        self.build_dir = self.project_root / "build"
        self.dist_dir = self.project_root / "dist"
        self.system = platform.system()

    def clean_build(self):
        """Clean previous build artifacts"""
        print("Cleaning previous builds...")
        if self.build_dir.exists():
            shutil.rmtree(self.build_dir)
        if self.dist_dir.exists():
            shutil.rmtree(self.dist_dir)

        self.build_dir.mkdir(exist_ok=True)
        self.dist_dir.mkdir(exist_ok=True)
        print("✓ Build directories cleaned")

    def check_dependencies(self) -> bool:
        """Check if required build dependencies are available"""
        print("Checking dependencies...")

        required = ['cmake']
        optional = ['git', 'python3']

        all_ok = True
        for cmd in required:
            if not shutil.which(cmd):
                print(f"✗ Missing required dependency: {cmd}")
                all_ok = False
            else:
                print(f"✓ Found {cmd}")

        for cmd in optional:
            if shutil.which(cmd):
                print(f"✓ Found {cmd}")

        return all_ok

    def configure_cmake(self, build_type: str = "Release") -> bool:
        """Configure CMake build"""
        print(f"\nConfiguring CMake ({build_type})...")

        cmake_args = [
            'cmake',
            '-B', str(self.build_dir),
            f'-DCMAKE_BUILD_TYPE={build_type}',
        ]

        # Platform-specific configuration
        if self.system == "Windows":
            cmake_args.extend([
                '-G', 'Visual Studio 17 2022',
                '-A', 'x64'
            ])

            # Try to use vcpkg if available
            vcpkg_path = Path(os.environ.get('VCPKG_ROOT', 'C:/vcpkg'))
            if vcpkg_path.exists():
                toolchain = vcpkg_path / 'scripts/buildsystems/vcpkg.cmake'
                cmake_args.append(f'-DCMAKE_TOOLCHAIN_FILE={toolchain}')

        elif self.system == "Darwin":  # macOS
            qt_path = subprocess.run(
                ['brew', '--prefix', 'qt@6'],
                capture_output=True,
                text=True
            )
            if qt_path.returncode == 0:
                qt_prefix = qt_path.stdout.strip()
                cmake_args.append(f'-DCMAKE_PREFIX_PATH={qt_prefix}')

        try:
            result = subprocess.run(
                cmake_args,
                cwd=self.project_root,
                check=True,
                capture_output=True,
                text=True
            )
            print("✓ CMake configuration successful")
            return True
        except subprocess.CalledProcessError as e:
            print(f"✗ CMake configuration failed: {e}")
            print(e.stderr)
            return False

    def build_project(self, build_type: str = "Release", jobs: int = None) -> bool:
        """Build the project"""
        print(f"\nBuilding project ({build_type})...")

        if jobs is None:
            jobs = os.cpu_count() or 1

        cmake_build_args = [
            'cmake',
            '--build', str(self.build_dir),
            '--config', build_type,
            '--parallel', str(jobs)
        ]

        try:
            result = subprocess.run(
                cmake_build_args,
                cwd=self.project_root,
                check=True,
                capture_output=True,
                text=True
            )
            print(f"✓ Build successful (using {jobs} parallel jobs)")
            return True
        except subprocess.CalledProcessError as e:
            print(f"✗ Build failed: {e}")
            print(e.stderr)
            return False

    def package_windows(self) -> Optional[Path]:
        """Package Windows executable with dependencies"""
        print("\nPackaging Windows executable...")

        exe_path = self.build_dir / "Release" / "DockerHomelabManager.exe"
        if not exe_path.exists():
            exe_path = self.build_dir / "DockerHomelabManager.exe"

        if not exe_path.exists():
            print("✗ Executable not found")
            return None

        # Create package directory
        package_dir = self.dist_dir / "DockerHomelabManager-Windows"
        package_dir.mkdir(parents=True, exist_ok=True)

        # Copy executable
        shutil.copy(exe_path, package_dir / "DockerHomelabManager.exe")

        # Run windeployqt if available
        if shutil.which('windeployqt'):
            subprocess.run(
                ['windeployqt', str(package_dir / "DockerHomelabManager.exe")],
                check=False
            )

        # Create ZIP archive
        zip_path = self.dist_dir / "DockerHomelabManager-Windows.zip"
        shutil.make_archive(
            str(zip_path.with_suffix('')),
            'zip',
            package_dir
        )

        print(f"✓ Windows package created: {zip_path}")
        return zip_path

    def package_linux(self) -> Optional[Path]:
        """Package Linux executable (AppImage if possible)"""
        print("\nPackaging Linux executable...")

        exe_path = self.build_dir / "DockerHomelabManager"
        if not exe_path.exists():
            print("✗ Executable not found")
            return None

        # Copy to dist
        dist_exe = self.dist_dir / "DockerHomelabManager-Linux"
        shutil.copy(exe_path, dist_exe)
        dist_exe.chmod(0o755)

        print(f"✓ Linux executable: {dist_exe}")

        # TODO: Create AppImage if linuxdeploy available
        return dist_exe

    def package_macos(self) -> Optional[Path]:
        """Package macOS application bundle"""
        print("\nPackaging macOS application...")

        app_path = self.build_dir / "DockerHomelabManager.app"
        if not app_path.exists():
            print("✗ Application bundle not found")
            return None

        # Copy to dist
        dist_app = self.dist_dir / "DockerHomelabManager.app"
        if dist_app.exists():
            shutil.rmtree(dist_app)
        shutil.copytree(app_path, dist_app)

        # TODO: Create DMG if possible
        print(f"✓ macOS application: {dist_app}")
        return dist_app

    def package(self) -> List[Path]:
        """Package executables for current platform"""
        packages = []

        if self.system == "Windows":
            pkg = self.package_windows()
            if pkg:
                packages.append(pkg)
        elif self.system == "Linux":
            pkg = self.package_linux()
            if pkg:
                packages.append(pkg)
        elif self.system == "Darwin":
            pkg = self.package_macos()
            if pkg:
                packages.append(pkg)

        return packages

    def run_tests(self) -> bool:
        """Run tests if available"""
        print("\nRunning tests...")

        test_exe = self.build_dir / "tests" / "DockerHomelabManager_tests"
        if not test_exe.exists() and self.system == "Windows":
            test_exe = self.build_dir / "tests" / "Release" / "DockerHomelabManager_tests.exe"

        if not test_exe.exists():
            print("⚠ No tests found (skipping)")
            return True

        try:
            subprocess.run([str(test_exe)], check=True)
            print("✓ All tests passed")
            return True
        except subprocess.CalledProcessError:
            print("✗ Tests failed")
            return False

    def full_build_and_package(self, clean: bool = True, build_type: str = "Release") -> bool:
        """Complete build and packaging pipeline"""
        print("="*60)
        print("DOCKER HOMELAB MANAGER - BUILD AUTOMATION")
        print("="*60)
        print(f"Platform: {self.system}")
        print(f"Build type: {build_type}")
        print(f"Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print("="*60 + "\n")

        # Check dependencies
        if not self.check_dependencies():
            print("\n✗ Missing required dependencies")
            return False

        # Clean if requested
        if clean:
            self.clean_build()

        # Configure
        if not self.configure_cmake(build_type):
            return False

        # Build
        if not self.build_project(build_type):
            return False

        # Test
        if not self.run_tests():
            print("\n⚠ Tests failed, but continuing with packaging...")

        # Package
        packages = self.package()

        print("\n" + "="*60)
        if packages:
            print("BUILD SUCCESSFUL!")
            print("="*60)
            print("\nPackages created:")
            for pkg in packages:
                print(f"  • {pkg.name}")
            print(f"\nLocation: {self.dist_dir}")
        else:
            print("BUILD COMPLETED (no packages created)")
            print("="*60)

        print("\n")
        return True

def main():
    """Main entry point"""
    import argparse

    parser = argparse.ArgumentParser(description="Build Automation")
    parser.add_argument("--no-clean", action="store_true",
                       help="Don't clean before building")
    parser.add_argument("--debug", action="store_true",
                       help="Build in Debug mode")
    parser.add_argument("--jobs", type=int,
                       help="Number of parallel build jobs")

    args = parser.parse_args()

    build_type = "Debug" if args.debug else "Release"

    automation = BuildAutomation()
    success = automation.full_build_and_package(
        clean=not args.no_clean,
        build_type=build_type
    )

    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
