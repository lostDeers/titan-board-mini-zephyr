#!/usr/bin/env python3
"""Conservative check for active dependencies on references/.

This script intentionally scans source/control files only. It is a guardrail for
low-level policy violations; it does not replace /reference-check.
"""

from __future__ import annotations

import fnmatch
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
REFERENCE_TOKEN = "references/"

INCLUDED_NAMES = {
    "CMakeLists.txt",
    "Kconfig",
    "prj.conf",
    "west.yml",
    "module.yml",
}
INCLUDED_SUFFIXES = {
    ".c",
    ".h",
    ".cpp",
    ".hpp",
    ".cc",
    ".cxx",
    ".dts",
    ".dtsi",
    ".overlay",
    ".conf",
    ".cmake",
    ".sh",
    ".py",
    ".yaml",
    ".yml",
}
SKIP_DIR_PATTERNS = {
    ".git",
    ".omp",
    "build",
    "build-*",
    "build_*",
    "references",
    "memory",
    "docs",
}


def should_skip_dir(path: pathlib.Path) -> bool:
    return any(fnmatch.fnmatch(path.name, pattern) for pattern in SKIP_DIR_PATTERNS)


def should_scan(path: pathlib.Path) -> bool:
    return path.name in INCLUDED_NAMES or path.suffix in INCLUDED_SUFFIXES


def main() -> int:
    violations: list[tuple[pathlib.Path, int, str]] = []
    for path in ROOT.rglob("*"):
        if path.is_dir():
            continue
        rel = path.relative_to(ROOT)
        if any(should_skip_dir(part) for part in rel.parents if part != pathlib.Path(".")):
            continue
        if not should_scan(path):
            continue
        try:
            lines = path.read_text(encoding="utf-8").splitlines()
        except UnicodeDecodeError:
            continue
        for number, line in enumerate(lines, start=1):
            if REFERENCE_TOKEN in line:
                violations.append((rel, number, line.strip()))

    if not violations:
        print("PASS: no active references/ dependencies found")
        return 0

    print("FAIL: active references/ dependencies found")
    for rel, number, line in violations:
        print(f"{rel}:{number}: {line}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
