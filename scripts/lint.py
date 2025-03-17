#!/usr/bin/env python3

import multiprocessing
import os
import re
import shutil
import subprocess

from flipper.app import App

SOURCE_CODE_FILE_EXTENSIONS = [".h", ".c", ".cpp", ".cxx", ".hpp"]
SOURCE_CODE_FILE_PATTERN = r"^[0-9A-Za-z_]+\.[a-z]+$"
SOURCE_CODE_DIR_PATTERN = r"^[0-9A-Za-z_]+$"


class Main(App):
    def init(self):
        self.subparsers = self.parser.add_subparsers(help="sub-command help")

        # generate
        self.parser_check = self.subparsers.add_parser(
            "check", help="Check source code format and file names"
        )
        self.parser_check.add_argument("input", nargs="+")
        self.parser_check.set_defaults(func=self.check)

        # merge
        self.parser_format = self.subparsers.add_parser(
            "format", help="Format source code and fix file names"
        )
        self.parser_format.add_argument(
            "input",
            nargs="+",
        )
        self.parser_format.set_defaults(func=self.format)
        
        # Add markdown linting subcommand
        self.parser_markdown = self.subparsers.add_parser(
            "markdown", help="Lint and fix markdown files"
        )
        self.parser_markdown.add_argument("--fix", action="store_true", help="Fix issues when possible")
        self.parser_markdown.add_argument("--config", help="Path to markdownlint config file")
        self.parser_markdown.add_argument("files", nargs="*", help="Files or directories to check")
        self.parser_markdown.set_defaults(func=self.markdown_lint)

        # Include markdown by default
        self.parser.add_argument(
            "--include-md", action="store_true", help="Include markdown files in linting", default=True
        )
        
        # Add containerization validation
        self.parser.add_argument(
            "--k8s", action="store_true", help="Validate containerization manifests"
        )

    @staticmethod
    def _filter_lint_directories(
        dirpath: str, dirnames: list[str], excludes: tuple[str]
    ):
        # Add markdown files to lint sources
        if self.args.include_md:
            for filename in os.listdir(dirpath):
                if filename.lower().endswith('.md') and not any(
                        ex in os.path.join(dirpath, filename) for ex in excludes):
                    self.md_sources.append(os.path.join(dirpath, filename))
        # Skipping 3rd-party code - usually resides in subfolder "lib"
        if "lib" in dirnames:
            dirnames.remove("lib")
        # Skipping hidden and excluded folders
        for dirname in dirnames.copy():
            if dirname.startswith("."):
                dirnames.remove(dirname)
            if os.path.join(dirpath, dirname).startswith(excludes):
                dirnames.remove(dirname)

    def _check_folders(self, folders: list, excludes: tuple[str]):
        show_message = False
        pattern = re.compile(SOURCE_CODE_DIR_PATTERN)
        for folder in folders:
            for dirpath, dirnames, filenames in os.walk(folder):
                self._filter_lint_directories(dirpath, dirnames, excludes)

                for dirname in dirnames:
                    if not pattern.match(dirname):
                        to_fix = os.path.join(dirpath, dirname)
                        self.logger.warning(f"Found incorrectly named folder {to_fix}")
                        show_message = True
        if show_message:
            self.logger.warning(
                "Folders are not renamed automatically, please fix it by yourself"
            )

    def _find_sources(self, folders: list, excludes: tuple[str]):
        output = []
        for folder in folders:
            for dirpath, dirnames, filenames in os.walk(folder):
                self._filter_lint_directories(dirpath, dirnames, excludes)

                for filename in filenames:
                    ext = os.path.splitext(filename.lower())[1]
                    if ext not in SOURCE_CODE_FILE_EXTENSIONS:
                        continue
                    output.append(os.path.join(dirpath, filename))
        return output

    @staticmethod
    def _format_source(task):
        try:
            subprocess.check_call(task)
            return True
        except subprocess.CalledProcessError:
            return False

    def _format_sources(self, sources: list, dry_run: bool = False):
        args = ["clang-format", "--Werror", "--style=file", "-i"]
        if dry_run:
            args.append("--dry-run")

        files_per_task = 69
        tasks = []
        while len(sources) > 0:
            tasks.append(args + sources[:files_per_task])
            sources = sources[files_per_task:]

        pool = multiprocessing.Pool()
        results = pool.map(self._format_source, tasks)

        return all(results)

    def _fix_filename(self, filename: str):
        return filename.replace("-", "_")

    def _replace_occurrence(self, sources: list, old: str, new: str):
        old = old.encode()
        new = new.encode()
        for source in sources:
            content = open(source, "rb").read()
            if content.count(old) > 0:
                self.logger.info(f"Replacing {old} with {new} in {source}")
                content = content.replace(old, new)
                open(source, "wb").write(content)

    def _apply_file_naming_convention(self, sources: list, dry_run: bool = False):
        pattern = re.compile(SOURCE_CODE_FILE_PATTERN)
        good = []
        bad = []
        # Check sources for invalid filenames
        for source in sources:
            basename = os.path.basename(source)
            if not pattern.match(basename):
                new_basename = self._fix_filename(basename)
                if not pattern.match(new_basename):
                    self.logger.error(f"Unable to fix name for {basename}")
                    return False
                bad.append((source, basename, new_basename))
            else:
                good.append(source)
        # Notify about errors or replace all occurrences
        if dry_run:
            if len(bad) > 0:
                self.logger.error(f"Found {len(bad)} incorrectly named files")
                self.logger.info(bad)
                return False
        else:
            # Replace occurrences in text files
            for source, old, new in bad:
                self._replace_occurrence(sources, old, new)
            # Rename files
            for source, old, new in bad:
                shutil.move(source, source.replace(old, new))
        return True

    def _apply_file_permissions(self, sources: list, dry_run: bool = False):
        execute_permissions = 0o111
        re.compile(SOURCE_CODE_FILE_PATTERN)
        good = []
        bad = []
        # Check sources for unexpected execute permissions
        for source in sources:
            st = os.stat(source)
            perms_too_many = st.st_mode & execute_permissions
            if perms_too_many:
                good_perms = st.st_mode & ~perms_too_many
                bad.append((source, oct(perms_too_many), good_perms))
            else:
                good.append(source)
        # Notify or fix
        if dry_run:
            if len(bad) > 0:
                self.logger.error(f"Found {len(bad)} incorrect permissions")
                self.logger.info([record[0:2] for record in bad])
                return False
        else:
            for source, perms_too_many, new_perms in bad:
                os.chmod(source, new_perms)
        return True

    def _perform(self, dry_run: bool):
        result = 0
        excludes = []
        for folder in self.args.input.copy():
            if folder.startswith("!"):
                excludes.append(folder.removeprefix("!"))
                self.args.input.remove(folder)
        excludes = tuple(excludes)
        sources = self._find_sources(self.args.input, excludes)
        if not self._format_sources(sources, dry_run=dry_run):
            result |= 0b001
        if not self._apply_file_naming_convention(sources, dry_run=dry_run):
            result |= 0b010
        if not self._apply_file_permissions(sources, dry_run=dry_run):
            result |= 0b100
        self._check_folders(self.args.input, excludes)
        return result

    def check(self):
        return self._perform(dry_run=True)

    def format(self):
        return self._perform(dry_run=False)

    def lint_markdown_files(self):
        """Run markdownlint on all markdown files."""
        if not self.md_sources:
            self.logger.info("No markdown files to lint")
            return 0
            
        self.logger.info(f"Linting {len(self.md_sources)} markdown files")
        
        markdownlint_config = os.path.join(self.project_dir, ".markdownlint.json")
        if not os.path.exists(markdownlint_config):
            self.logger.warning("No .markdownlint.json found, using default settings")
        
        result = subprocess.run(
            ["npx", "markdownlint-cli", "--config", markdownlint_config, *self.md_sources],
            capture_output=True,
            text=True,
        )
        
        if result.returncode != 0:
            self.logger.error(f"Markdown lint errors found:\n{result.stdout}")
            return result.returncode
        
        self.logger.info("Markdown lint passed")
        return 0

    def markdown_lint(self):
        """Run markdownlint on markdown files"""
        config_path = self.args.config or os.path.join(self.project_dir, ".markdownlint.json")
        if not os.path.exists(config_path):
            self.logger.warning(f"Markdown lint config not found at {config_path}")
            config_path = os.path.join(os.path.dirname(__file__), "default_markdownlint.json")
            
        files = self.args.files
        if not files:
            # Default to finding all .md files
            files = []
            for root, _, filenames in os.walk(self.project_dir):
                for filename in filenames:
                    if filename.lower().endswith('.md'):
                        files.append(os.path.join(root, filename))
        
        if not files:
            self.logger.info("No markdown files found")
            return 0
            
        cmd = ["npx", "markdownlint-cli"]
        if self.args.fix:
            cmd.append("--fix")
        cmd.extend(["--config", config_path])
        cmd.extend(files)
        
        self.logger.info(f"Running markdownlint on {len(files)} files")
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            self.logger.error(f"Markdown lint failed: {result.stdout}")
            return result.returncode
        
        self.logger.info("Markdown lint passed")
        return 0

    # Add containerization manifest validation
    def validate_k8s_manifests(self):
        """Validate Kubernetes-style manifests for syntax and best practices"""
        import json
        
        manifests_dir = os.path.join(self.project_dir, "assets/resources/containerization")
        if not os.path.exists(manifests_dir):
            self.logger.info("No containerization manifests directory found")
            return 0
            
        manifest_files = [f for f in os.listdir(manifests_dir) if f.endswith('.json')]
        if not manifest_files:
            self.logger.info("No manifest files found")
            return 0
            
        errors = 0
        for manifest_file in manifest_files:
            try:
                with open(os.path.join(manifests_dir, manifest_file)) as f:
                    manifest = json.load(f)
                    
                # Validate required fields
                if "apiVersion" not in manifest:
                    self.logger.error(f"{manifest_file}: Missing apiVersion")
                    errors += 1
                if "kind" not in manifest:
                    self.logger.error(f"{manifest_file}: Missing kind")
                    errors += 1
                if "metadata" not in manifest:
                    self.logger.error(f"{manifest_file}: Missing metadata")
                    errors += 1
                elif "name" not in manifest["metadata"]:
                    self.logger.error(f"{manifest_file}: Missing metadata.name")
                    errors += 1
                    
                # Validate pod spec
                if manifest.get("kind") == "Pod" and "spec" in manifest:
                    if "containers" not in manifest["spec"]:
                        self.logger.error(f"{manifest_file}: Missing spec.containers")
                        errors += 1
                    elif not manifest["spec"]["containers"]:
                        self.logger.error(f"{manifest_file}: Empty containers list")
                        errors += 1
                    
                    # Check each container
                    for i, container in enumerate(manifest["spec"].get("containers", [])):
                        if "name" not in container:
                            self.logger.error(f"{manifest_file}: Container {i} missing name")
                            errors += 1
                        if "image" not in container:
                            self.logger.error(f"{manifest_file}: Container {i} missing image")
                            errors += 1
                            
                # Check resource limits - this is especially important for Flipper Zero
                for i, container in enumerate(manifest.get("spec", {}).get("containers", [])):
                    if "resources" not in container:
                        self.logger.warning(f"{manifest_file}: Container {i} ({container.get('name', 'unnamed')}) has no resource limits")
                    elif "limits" not in container["resources"]:
                        self.logger.warning(f"{manifest_file}: Container {i} ({container.get('name', 'unnamed')}) has no resource limits")
                        
            except json.JSONDecodeError as e:
                self.logger.error(f"{manifest_file}: Invalid JSON: {e}")
                errors += 1
                
        if errors:
            self.logger.error(f"Found {errors} errors in containerization manifests")
            return errors
        else:
            self.logger.info("All containerization manifests are valid")
            return 0
        
    def main(self):
        if hasattr(self.args, "func"):
            result = self.args.func()
            
            # Always validate K8s manifests if --k8s is specified
            if getattr(self.args, "k8s", False):
                k8s_result = self.validate_k8s_manifests()
                if k8s_result != 0:
                    result = k8s_result
                    
            return result
        else:
            self.parser.print_help()
            return 0


if __name__ == "__main__":
    Main()()
