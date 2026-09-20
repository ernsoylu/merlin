"""Byte-exact fixture comparison for generated reference trees."""

from __future__ import annotations

import difflib
from pathlib import Path


def _files(root: Path) -> set[Path]:
    return {path.relative_to(root) for path in root.rglob("*") if path.is_file()}


def tree_diff(actual: Path, expected: Path) -> str:
    lines = []
    actual_files = _files(actual)
    expected_files = _files(expected)
    for path in sorted(expected_files - actual_files):
        lines.append(f"missing: {path}")
    for path in sorted(actual_files - expected_files):
        lines.append(f"unexpected: {path}")
    for path in sorted(actual_files & expected_files):
        left = (expected / path).read_bytes()
        right = (actual / path).read_bytes()
        if left == right:
            continue
        lines.append(f"changed: {path}")
        try:
            before = left.decode().splitlines(keepends=True)
            after = right.decode().splitlines(keepends=True)
        except UnicodeDecodeError:
            lines.append("  binary content differs")
        else:
            lines.extend(difflib.unified_diff(before, after, fromfile=f"expected/{path}", tofile=f"actual/{path}"))
    return "".join(lines)


def assert_tree_equal(actual: Path, expected: Path) -> None:
    diff = tree_diff(actual, expected)
    assert not diff, diff
