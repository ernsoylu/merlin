#!/usr/bin/env python3
"""Build and run the non-physical ESP32 QEMU scenario catalog."""

from __future__ import annotations

import argparse
import json
import os
import shlex
import shutil
import subprocess
import tempfile
from pathlib import Path

from run_smoke import assert_scenario, qemu_output


ROOT = Path(__file__).resolve().parents[2]
IDF = Path(os.environ.get("IDF_PATH", Path.home() / "esp/esp-idf"))


def _idf_python() -> Path:
    configured = os.environ.get("IDF_PYTHON_ENV_PATH")
    if configured:
        return Path(configured) / "bin/python"
    candidates = sorted((Path.home() / ".espressif/python_env").glob("idf*_py*/bin/python"), reverse=True)
    if not candidates:
        raise FileNotFoundError("ESP-IDF Python environment not found")
    return candidates[0]


def _scenario_defaults(name: str) -> list[str]:
    scenarios = json.loads((Path(__file__).parent / "scenarios.json").read_text(encoding="utf-8"))
    item = next(item for item in scenarios if item["name"] == name)
    return [entry if entry.startswith("CONFIG_") else f"CONFIG_{entry}"
            for entry in item["configuration"].split(";") if entry]


def _build(source: Path, scenario: str, work: Path) -> Path:
    copied = work / "source"
    shutil.copytree(source, copied, ignore=shutil.ignore_patterns("build", "sdkconfig", "__pycache__"))
    defaults = work / "sdkconfig.defaults"
    defaults.write_text((source / "sdkconfig.defaults").read_text(encoding="utf-8") + "\n" + "\n".join(_scenario_defaults(scenario)) + "\n", encoding="utf-8")
    build = work / "build"
    sdkconfig = work / "sdkconfig"
    command = "source {export}; cd {source}; idf.py -B {build} -D SDKCONFIG={sdkconfig} -D SDKCONFIG_DEFAULTS={defaults} build".format(
        export=shlex.quote(str(IDF / "export.sh")),
        source=shlex.quote(str(copied)),
        build=shlex.quote(str(build)),
        sdkconfig=shlex.quote(str(sdkconfig)),
        defaults=shlex.quote(str(defaults)),
    )
    subprocess.run(["bash", "-lc", command], check=True)
    app = next(path for path in build.glob("*.bin") if path.name != "qemu-flash.bin")
    qemu_flash = build / "qemu-flash.bin"
    subprocess.run([str(_idf_python()), "-m", "esptool", "--chip", "esp32", "merge_bin", "-o", str(qemu_flash), "--flash_mode", "dio", "--flash_freq", "40m", "--flash_size", "4MB", "0x1000", str(build / "bootloader/bootloader.bin"), "0x10000", str(app), "0x8000", str(build / "partition_table/partition-table.bin")], check=True, stdout=subprocess.DEVNULL)
    return build


def run_scenario(name: str, seconds: float) -> int:
    with tempfile.TemporaryDirectory(prefix=f"merlin-qemu-{name}-") as directory:
        build = _build(ROOT / "v01-reference", name, Path(directory))
        return assert_scenario(qemu_output(build, seconds), name)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--scenario", action="append", choices=("normal", "fake-disconnect-recovery", "stale-failsafe", "deadline-skip", "controlled-reset-safe-halt"))
    parser.add_argument("--seconds", type=float, default=3.0)
    args = parser.parse_args()
    scenarios = args.scenario or ["normal", "fake-disconnect-recovery", "stale-failsafe", "deadline-skip"]
    for name in scenarios:
        count = run_scenario(name, args.seconds)
        print(f"qemu {name} ok: {count} structured records")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
