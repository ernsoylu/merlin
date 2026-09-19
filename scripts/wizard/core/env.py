"""Toolchain discovery and verification (the `check-env` command).

Everything pinned lives in toolchain.env at the repository root; this module is
the only thing that reads it, so install.sh and check-env cannot drift apart.
"""

import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
TOOLCHAIN_ENV = REPO_ROOT / "toolchain.env"
IDF_TOOL = "idf.py"
ESPRESSIF_DIR = ".espressif"


def load_toolchain(path=None):
    """Parse toolchain.env. Same grammar the shell sees: KEY=value, # comments."""
    path = Path(path) if path else TOOLCHAIN_ENV
    pins = {}
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        key, _, value = line.partition("=")
        pins[key.strip()] = value.strip()
    return pins


def parse_version(text):
    """First dotted version in `text`, or None.

    Tool banners vary: 'ESP-IDF v5.2.3', 'ESP-IDF v5.2.3-dirty',
    'qemu-system-xtensa version 8.2.0 (v8.2.0-...)'. Take the first triple and
    ignore whatever the build appended.
    """
    text = text or ""
    for start, character in enumerate(text):
        if not character.isdigit():
            continue
        parts = []
        position = start
        while len(parts) < 3:
            begin = position
            while position < len(text) and text[position].isdigit():
                position += 1
            if begin == position:
                break
            parts.append(text[begin:position])
            if len(parts) < 3:
                if position >= len(text) or text[position] != ".":
                    break
                position += 1
        if len(parts) == 3:
            return ".".join(parts)
    return None


def _run(cmd):
    """Capture a tool's version banner. Returns '' if the tool is unusable."""
    try:
        done = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    except (OSError, subprocess.SubprocessError):
        return ""
    return (done.stdout or "") + (done.stderr or "")


def find_idf():
    """idf.py from PATH, else from IDF_PATH, else the conventional clone."""
    on_path = shutil.which(IDF_TOOL)
    if on_path:
        return Path(on_path)
    roots = [os.environ.get("IDF_PATH"), Path.home() / "esp" / "esp-idf"]
    for root in roots:
        if root and (Path(root) / "tools" / IDF_TOOL).is_file():
            return Path(root) / "tools" / IDF_TOOL
    return None


def find_qemu(pins):
    """Prefer Espressif QEMU; a distro binary may lack the esp32 machine."""
    tools = Path.home() / ESPRESSIF_DIR / "tools" / pins.get("QEMU_TOOL", "qemu-xtensa")
    found = sorted(tools.glob("**/qemu-system-xtensa"))
    if found:
        return found[-1]
    on_path = shutil.which("qemu-system-xtensa")
    return Path(on_path) if on_path else None


def find_gcc(pins):
    """Find the Espressif compiler even when export.sh has not been sourced."""
    name = f"{pins['GCC_PREFIX']}-gcc"
    tools = Path.home() / ESPRESSIF_DIR / "tools" / "xtensa-esp-elf"
    found = sorted(tools.glob(f"**/bin/{name}"))
    if found:
        return found[-1]
    on_path = shutil.which(name)
    return Path(on_path) if on_path else None


def find_idf_python():
    """Return the Python environment installed by ESP-IDF, if available."""
    configured = os.environ.get("IDF_PYTHON_ENV_PATH")
    candidates = []
    if configured:
        candidates.append(Path(configured) / "bin" / "python")
    candidates.extend(
        sorted(
            (Path.home() / ESPRESSIF_DIR / "python_env").glob("idf*_py*/bin/python"),
            reverse=True,
        )
    )
    return next((candidate for candidate in candidates if candidate.is_file()), None)


def check_python(pins):
    want = tuple(int(part) for part in pins["PYTHON_MIN"].split("."))
    have = sys.version_info[: len(want)]
    version = ".".join(str(part) for part in have)
    return have >= want, f"{version} (need >= {pins['PYTHON_MIN']})"


def check_packages():
    """Every pin in requirements.txt importable at exactly the pinned version."""
    try:
        from importlib.metadata import PackageNotFoundError, version
    except ImportError:  # pragma: no cover - python < 3.8 cannot reach here
        return False, "importlib.metadata unavailable"

    results = []
    ok = True
    for line in (REPO_ROOT / "requirements.txt").read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        name, _, want = line.partition("==")
        try:
            have = version(name)
        except PackageNotFoundError:
            results.append(f"{name} MISSING")
            ok = False
            continue
        if have != want:
            results.append(f"{name} {have} (want {want})")
            ok = False
        else:
            results.append(f"{name} {have}")
    return ok, ", ".join(results)


def check_env(stream=sys.stdout):
    """Report every finding, not just pass/fail.

    A version mismatch has to be diagnosable from this output alone, so each
    row prints what was found and where it was found.
    """
    pins = load_toolchain()
    rows = []

    rows.append(("python", *check_python(pins)))
    rows.append(("packages", *check_packages()))

    idf = find_idf()
    if idf is None:
        rows.append(("esp-idf", False, f"{IDF_TOOL} not found (PATH, IDF_PATH, ~/esp/esp-idf)"))
    else:
        idf_python = find_idf_python() or Path(sys.executable)
        found = parse_version(_run([str(idf_python), str(idf), "--version"]))
        want = pins["ESP_IDF_VERSION"]
        rows.append(("esp-idf", found == want, f"{found or 'unknown'} at {idf} (want {want})"))

    gcc = f"{pins['GCC_PREFIX']}-gcc"
    gcc_path = find_gcc(pins)
    if gcc_path:
        rows.append(("toolchain", True, f"{parse_version(_run([str(gcc_path), '--version']))} at {gcc_path}"))
    else:
        rows.append(("toolchain", False, f"{gcc} not on PATH (source $IDF_PATH/export.sh)"))

    qemu = find_qemu(pins)
    if qemu is None:
        rows.append(("qemu", False, "qemu-system-xtensa not found (idf_tools.py install qemu-xtensa)"))
    else:
        # A distro qemu-system-xtensa exists on many machines and has no esp32
        # machine -- passing on presence alone is a false green.
        has_esp32 = "esp32" in _run([str(qemu), "-machine", "help"]).lower()
        found = parse_version(_run([str(qemu), "--version"]))
        detail = f"{found} at {qemu}"
        if not has_esp32:
            detail += " -- no esp32 machine, not the Espressif build"
        rows.append(("qemu", has_esp32, detail))

    width = max(len(name) for name, _, _ in rows)
    for name, ok, detail in rows:
        print(f"  {'ok  ' if ok else 'FAIL'}  {name:<{width}}  {detail}", file=stream)

    failed = [name for name, ok, _ in rows if not ok]
    if failed:
        print(f"\ncheck-env: {len(failed)} problem(s): {', '.join(failed)}", file=stream)
        return 2
    print("\ncheck-env: toolchain matches toolchain.env", file=stream)
    return 0
