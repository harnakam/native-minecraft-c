"""Fail publication if the Git index contains anything outside original source/docs."""
import pathlib
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[1]
git = ["git", "-c", f"safe.directory={root.as_posix()}"]
entries = subprocess.check_output(git + ["ls-files", "--stage", "-z"], cwd=root).decode("utf-8").split("\0")
paths = []
top_files = {".gitattributes", ".gitignore", "CMakeLists.txt", "README.md", "LICENSE"}
allowed = {"src": {".c", ".h"}, "tests": {".c", ".py", ".cjs"}, "scripts": {".py", ".ps1", ".sh"}, "docs": {".md"}}
violations = []
for entry in filter(None, entries):
    metadata, name = entry.split("\t", 1)
    mode, blob, stage = metadata.split()
    paths.append(name)
    if mode not in {"100644", "100755"} or stage != "0":
        violations.append(name + " (symlink, submodule or unresolved index stage)")
        continue
    p = pathlib.PurePosixPath(name)
    ok = name in top_files or (p.parts[0] in allowed and p.suffix in allowed[p.parts[0]])
    ok = ok or (p.parts[:2] == (".github", "workflows") and p.suffix in {".yml", ".yaml"})
    if not ok:
        violations.append(name)
        continue
    # A UTF-8 source allowlist also rejects accidental executable/archive/binary contents.
    content = subprocess.check_output(git + ["cat-file", "blob", blob], cwd=root)
    try:
        text = content.decode("utf-8")
        if "\x00" in text:
            violations.append(name + " (NUL byte)")
    except UnicodeDecodeError:
        violations.append(name + " (binary content)")
if violations:
    print("Public index audit FAILED:\n" + "\n".join(violations), file=sys.stderr)
    sys.exit(1)
if not any(p.startswith("src/") for p in paths):
    sys.exit("No source files staged")
print(f"Public index audit passed: {sum(bool(p) for p in paths)} UTF-8 source/config/docs files; no MCP, assets, jars or saves.")
