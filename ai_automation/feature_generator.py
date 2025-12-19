#!/usr/bin/env python3
"""
AI Feature Generator for Docker Homelab Manager
Continuously generates and implements features using Claude AI
"""

import os
import json
import anthropic
from pathlib import Path
from datetime import datetime
from typing import List, Dict, Optional
import subprocess
import sys

class AIFeatureGenerator:
    """AI-powered feature generation and implementation system"""

    def __init__(self, api_key: Optional[str] = None):
        self.api_key = api_key or os.environ.get("ANTHROPIC_API_KEY")
        if not self.api_key:
            raise ValueError("ANTHROPIC_API_KEY environment variable required")

        self.client = anthropic.Anthropic(api_key=self.api_key)
        self.project_root = Path(__file__).parent.parent
        self.todo_file = self.project_root / "TODO.md"
        self.features_log = self.project_root / "ai_automation" / "features_log.json"
        self.model = "claude-sonnet-4-5-20250929"

    def load_codebase_context(self) -> str:
        """Load relevant codebase files for context"""
        context = []

        # Load key architecture files
        arch_file = self.project_root / "docs" / "ARCHITECTURE.md"
        if arch_file.exists():
            context.append(f"=== ARCHITECTURE ===\n{arch_file.read_text()}\n")

        roadmap_file = self.project_root / "docs" / "ROADMAP.md"
        if roadmap_file.exists():
            context.append(f"=== ROADMAP ===\n{roadmap_file.read_text()}\n")

        # Load current TODO list
        if self.todo_file.exists():
            context.append(f"=== CURRENT TODO ===\n{self.todo_file.read_text()}\n")

        return "\n".join(context)

    def get_next_feature_to_implement(self) -> Optional[Dict]:
        """Use AI to determine the next best feature to implement"""
        context = self.load_codebase_context()

        prompt = f"""You are an AI software architect working on the Docker Homelab Manager project.

{context}

Based on the architecture, roadmap, and current TODO list, determine the NEXT HIGHEST PRIORITY feature to implement.

Consider:
1. Dependencies (implement foundational features first)
2. Impact (high-value features)
3. Complexity (balance quick wins with important complex features)
4. Current progress

Respond with a JSON object:
{{
    "feature_name": "Name of the feature",
    "priority": "high|medium|low",
    "component": "Component name (e.g., DockerClient, PortScanner)",
    "file_path": "Path to file to modify (e.g., src/docker/DockerClient.cpp)",
    "description": "Brief description of what to implement",
    "estimated_lines": 100,
    "dependencies": ["list", "of", "dependencies"],
    "rationale": "Why this feature should be next"
}}"""

        message = self.client.messages.create(
            model=self.model,
            max_tokens=2000,
            messages=[{"role": "user", "content": prompt}]
        )

        response_text = message.content[0].text
        # Extract JSON from response
        json_start = response_text.find('{')
        json_end = response_text.rfind('}') + 1
        if json_start >= 0 and json_end > json_start:
            return json.loads(response_text[json_start:json_end])
        return None

    def generate_feature_code(self, feature: Dict) -> str:
        """Generate code implementation for the feature using AI"""
        context = self.load_codebase_context()

        # Load the target file if it exists
        file_path = self.project_root / feature['file_path']
        current_code = ""
        if file_path.exists():
            current_code = file_path.read_text()

        prompt = f"""You are implementing a feature for the Docker Homelab Manager C++17 project.

PROJECT CONTEXT:
{context}

FEATURE TO IMPLEMENT:
{json.dumps(feature, indent=2)}

CURRENT FILE CONTENT ({feature['file_path']}):
```cpp
{current_code}
```

Generate the COMPLETE updated file content with the feature implemented.
Requirements:
- Use C++17 features (smart pointers, std::optional, structured bindings)
- Follow existing code style and patterns
- Add comprehensive error handling
- Include detailed comments
- Use the Logger for all logging (LOG_INFO, LOG_ERROR, etc.)
- Implement all TODO sections related to this feature
- Ensure thread safety where applicable

Respond with ONLY the complete C++ code, no explanations."""

        message = self.client.messages.create(
            model=self.model,
            max_tokens=8000,
            messages=[{"role": "user", "content": prompt}]
        )

        return message.content[0].text

    def update_todo_list(self, feature: Dict, status: str):
        """Update the TODO.md file with feature progress"""
        if not self.todo_file.exists():
            self.todo_file.write_text("# Docker Homelab Manager - TODO List\n\n")

        content = self.todo_file.read_text()
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        # Add entry for this feature
        entry = f"\n## {feature['feature_name']} - {status}\n"
        entry += f"- **Component**: {feature['component']}\n"
        entry += f"- **File**: {feature['file_path']}\n"
        entry += f"- **Status**: {status}\n"
        entry += f"- **Timestamp**: {timestamp}\n"
        entry += f"- **Description**: {feature['description']}\n\n"

        content += entry
        self.todo_file.write_text(content)

    def log_feature(self, feature: Dict, status: str, code_generated: bool = False):
        """Log feature generation to JSON file"""
        log_data = []
        if self.features_log.exists():
            log_data = json.loads(self.features_log.read_text())

        log_entry = {
            "timestamp": datetime.now().isoformat(),
            "feature": feature,
            "status": status,
            "code_generated": code_generated
        }
        log_data.append(log_entry)

        self.features_log.parent.mkdir(exist_ok=True)
        self.features_log.write_text(json.dumps(log_data, indent=2))

    def implement_feature(self, feature: Dict) -> bool:
        """Implement a single feature"""
        try:
            print(f"\n{'='*60}")
            print(f"Implementing: {feature['feature_name']}")
            print(f"Component: {feature['component']}")
            print(f"Priority: {feature['priority']}")
            print(f"{'='*60}\n")

            # Update TODO - In Progress
            self.update_todo_list(feature, "IN PROGRESS")
            self.log_feature(feature, "started")

            # Generate code
            print("Generating code with AI...")
            code = self.generate_feature_code(feature)

            # Extract code from markdown if present
            if "```cpp" in code:
                code = code.split("```cpp")[1].split("```")[0].strip()
            elif "```" in code:
                code = code.split("```")[1].split("```")[0].strip()

            # Write code to file
            file_path = self.project_root / feature['file_path']
            file_path.parent.mkdir(parents=True, exist_ok=True)
            file_path.write_text(code)
            print(f"✓ Code written to {feature['file_path']}")

            # Update TODO - Completed
            self.update_todo_list(feature, "COMPLETED")
            self.log_feature(feature, "completed", code_generated=True)

            return True

        except Exception as e:
            print(f"✗ Error implementing feature: {e}")
            self.update_todo_list(feature, f"FAILED: {str(e)}")
            self.log_feature(feature, f"failed: {str(e)}")
            return False

    def commit_and_push(self, feature: Dict):
        """Commit changes to git and push"""
        try:
            # Git add
            subprocess.run(["git", "add", "."], cwd=self.project_root, check=True)

            # Git commit
            commit_msg = f"AI: Implement {feature['feature_name']} in {feature['component']}"
            subprocess.run(
                ["git", "commit", "-m", commit_msg],
                cwd=self.project_root,
                check=True
            )

            # Git push
            branch = subprocess.run(
                ["git", "rev-parse", "--abbrev-ref", "HEAD"],
                cwd=self.project_root,
                capture_output=True,
                text=True,
                check=True
            ).stdout.strip()

            subprocess.run(
                ["git", "push", "-u", "origin", branch],
                cwd=self.project_root,
                check=True
            )

            print(f"✓ Changes committed and pushed to {branch}")
            return True

        except subprocess.CalledProcessError as e:
            print(f"✗ Git operation failed: {e}")
            return False

    def run_continuous(self, max_features: int = 10, auto_commit: bool = True):
        """Continuously generate and implement features"""
        print("="*60)
        print("AI FEATURE GENERATOR - CONTINUOUS MODE")
        print("="*60)
        print(f"Max features: {max_features}")
        print(f"Auto-commit: {auto_commit}")
        print(f"Model: {self.model}\n")

        features_implemented = 0

        while features_implemented < max_features:
            print(f"\n--- Feature {features_implemented + 1}/{max_features} ---")

            # Get next feature
            print("Analyzing codebase and determining next feature...")
            feature = self.get_next_feature_to_implement()

            if not feature:
                print("No more features to implement!")
                break

            # Implement feature
            success = self.implement_feature(feature)

            if success:
                features_implemented += 1

                # Commit and push
                if auto_commit:
                    print("Committing changes to GitHub...")
                    self.commit_and_push(feature)
            else:
                print("Feature implementation failed, continuing...")

        print(f"\n{'='*60}")
        print(f"COMPLETED: {features_implemented} features implemented")
        print(f"{'='*60}\n")

def main():
    """Main entry point"""
    import argparse

    parser = argparse.ArgumentParser(description="AI Feature Generator")
    parser.add_argument("--max-features", type=int, default=10,
                       help="Maximum features to implement")
    parser.add_argument("--no-commit", action="store_true",
                       help="Don't auto-commit changes")
    parser.add_argument("--single", action="store_true",
                       help="Implement only one feature")

    args = parser.parse_args()

    try:
        generator = AIFeatureGenerator()

        if args.single:
            feature = generator.get_next_feature_to_implement()
            if feature:
                generator.implement_feature(feature)
                if not args.no_commit:
                    generator.commit_and_push(feature)
        else:
            generator.run_continuous(
                max_features=args.max_features,
                auto_commit=not args.no_commit
            )

    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
