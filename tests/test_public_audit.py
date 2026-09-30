"""Exercise publication auditing against real, isolated Git index fixtures."""
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
AUDIT = ROOT / "scripts" / "audit_public.py"


class PublicIndexAuditTests(unittest.TestCase):
    def setUp(self):
        fixture_base = (ROOT / ".local").resolve()
        if not fixture_base.is_relative_to(ROOT.resolve()):
            raise RuntimeError("Audit fixtures must remain inside the workspace")
        fixture_base.mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(
            prefix="public-audit-test-", dir=fixture_base
        )
        self.repository = pathlib.Path(self.temporary.name).resolve()
        if not self.repository.is_relative_to(fixture_base):
            raise RuntimeError("Refusing cleanup outside the audit fixture directory")
        self.addCleanup(self.temporary.cleanup)
        self.environment = {
            key: value for key, value in os.environ.items()
            if not key.upper().startswith("GIT_")
        }
        self.environment.update({
            "GIT_CONFIG_NOSYSTEM": "1",
            "GIT_CONFIG_GLOBAL": os.devnull,
            "GIT_TERMINAL_PROMPT": "0",
        })
        self.git("init", "--quiet", "--template=")
        self.git("config", "--local", "user.name", "C919 Audit Test")
        self.git("config", "--local", "user.email", "audit-test@example.invalid")
        self.write(".gitignore", "/MCP-919/\n/assets/\n")
        self.write("README.md", "# Original audit fixture\n")
        self.write("docs/contract.md", "An independently authored test document.\n")
        self.write("src/original.c", "int original_value(void) { return 47; }\n")
        copied_audit = self.repository / "scripts" / "audit_public.py"
        copied_audit.parent.mkdir(parents=True)
        shutil.copyfile(AUDIT, copied_audit)
        self.git("add", "--all")

    def git(self, *arguments):
        return subprocess.run(
            ["git", "-c", f"safe.directory={self.repository.as_posix()}", *arguments],
            cwd=self.repository, env=self.environment, check=True,
            capture_output=True, text=True, encoding="utf-8", timeout=10,
        )

    def write(self, relative_path, content):
        path = self.repository / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(content, bytes):
            path.write_bytes(content)
        else:
            path.write_text(content, encoding="utf-8", newline="\n")
        return path

    def audit(self):
        return subprocess.run(
            [sys.executable, str(self.repository / "scripts" / "audit_public.py")],
            cwd=self.repository, env=self.environment, capture_output=True,
            text=True, encoding="utf-8", timeout=10,
        )

    def assert_rejected(self, result, diagnostic):
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn(diagnostic, result.stderr)

    def test_original_staged_source_and_docs_pass_with_private_untracked_files(self):
        self.write("MCP-919/private.txt", "A private fixture; never staged.\n")
        self.write("assets/private.bin", b"\x00\xff")
        result = self.audit()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Public index audit passed", result.stdout)

    def test_staged_binary_is_rejected_after_working_copy_is_replaced_with_text(self):
        self.write("src/original.c", b"\xff\xfe\x80\x01")
        self.git("add", "--", "src/original.c")
        clean = "int original_value(void) { return 47; }\n"
        self.write("src/original.c", clean)
        self.assertEqual((self.repository / "src/original.c").read_text(), clean)
        self.assert_rejected(self.audit(), "src/original.c (binary content)")

    def test_staged_nul_is_rejected_even_when_bytes_are_valid_utf8(self):
        self.write("src/original.c", b"int original_value;\x00\n")
        self.git("add", "--", "src/original.c")
        self.write("src/original.c", "int original_value;\n")
        self.assert_rejected(self.audit(), "src/original.c (NUL byte)")

    def test_forbidden_ignored_path_is_rejected_when_force_staged(self):
        self.write("MCP-919/source.md", "An invented fixture under a forbidden path.\n")
        self.git("check-ignore", "--", "MCP-919/source.md")
        self.git("add", "--force", "--", "MCP-919/source.md")
        self.assert_rejected(self.audit(), "MCP-919/source.md")

    def test_unstaged_binary_does_not_replace_the_allowed_staged_blob(self):
        self.write("src/original.c", b"\xff\x00\xfe")
        result = self.audit()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_document_only_index_is_rejected_without_source(self):
        self.git("rm", "--cached", "--", "src/original.c")
        self.assert_rejected(self.audit(), "No source files staged")


if __name__ == "__main__":
    unittest.main()
