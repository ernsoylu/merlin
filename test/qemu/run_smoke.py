#!/usr/bin/env python3
"""Run the ESP32 QEMU smoke image and assert structured runtime output."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path


def find_qemu() -> Path:
    candidates = []
    configured = os.environ.get("QEMU_SYSTEM_XTENSA")
    if configured:
        candidates.append(Path(configured))
    candidates.append(Path.home() / ".espressif/tools/qemu-xtensa/esp_develop_9.0.0_20240606/qemu/bin/qemu-system-xtensa")
    on_path = shutil.which("qemu-system-xtensa")
    if on_path:
        candidates.append(Path(on_path))
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise FileNotFoundError("Espressif qemu-system-xtensa not found")


def records_from(output: str) -> list[dict]:
    return [json.loads(line) for line in output.splitlines() if line.lstrip().startswith("{")]


def assert_records(output: str) -> int:
    records = records_from(output)
    by_instance = {}
    for record in records:
        if "instance" in record:
            by_instance.setdefault(record["instance"], []).append(record)
    assert set(by_instance) >= {"ambientSensor", "enclosureSensor"}
    for name, items in by_instance.items():
        assert all(item["health"] == "READY" for item in items), name
        sequences = [item["sequence"] for item in items]
        assert sequences == sorted(set(sequences)), name
    assert "panic" not in output.lower()
    return len(records)


def assert_scenario(output: str, scenario: str) -> int:
    if scenario == "normal":
        return assert_records(output)
    records = records_from(output)
    by_instance = {}
    for record in records:
        if "instance" in record:
            by_instance.setdefault(record["instance"], []).append(record)
    if scenario == "fake-disconnect-recovery":
        assert len(by_instance.get("enclosureSensor", [])) > len(by_instance.get("ambientSensor", [])) >= 3
        assert "panic" not in output.lower()
    elif scenario == "stale-failsafe":
        assert len(by_instance.get("ambientSensor", [])) <= 4
        assert len(by_instance.get("enclosureSensor", [])) > 10
    elif scenario == "deadline-skip":
        assert any(record.get("fault") == "RTF-002" for record in records)
    elif scenario == "controlled-reset-safe-halt":
        assert any(record.get("system") == "SAFE_HALT" for record in records)
    else:
        raise ValueError(f"unknown scenario: {scenario}")
    return len(records)


def qemu_output(build_dir: Path, seconds: float = 3.0) -> str:
    source = build_dir / "qemu-flash.bin"
    if not source.is_file():
        raise FileNotFoundError(source)
    qemu = find_qemu()
    with tempfile.TemporaryDirectory(prefix="merlin-qemu-") as temp:
        image = Path(temp) / "flash.bin"
        image.write_bytes(source.read_bytes())
        with image.open("ab") as stream:
            stream.truncate(4 * 1024 * 1024)
        command = [str(qemu), "-M", "esp32", "-nographic", "-drive", f"file={image},if=mtd,format=raw"]
        try:
            completed = subprocess.run(command, capture_output=True, text=True, timeout=seconds)
            output = (completed.stdout or "") + (completed.stderr or "")
        except subprocess.TimeoutExpired as error:
            output = (error.stdout or "") + (error.stderr or "")
            if isinstance(output, bytes):
                output = output.decode(errors="replace")
    return output


def run(build_dir: Path, seconds: float = 3.0, scenario: str = "normal") -> int:
    count = assert_scenario(qemu_output(build_dir, seconds), scenario)
    print(f"qemu {scenario} ok: {count} structured records")
    return count


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=Path("v01-reference/build"))
    parser.add_argument("--seconds", type=float, default=3.0)
    parser.add_argument("--scenario", default="normal", choices=("normal", "fake-disconnect-recovery", "stale-failsafe", "deadline-skip", "controlled-reset-safe-halt"))
    args = parser.parse_args()
    run(args.build_dir, args.seconds, args.scenario)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
