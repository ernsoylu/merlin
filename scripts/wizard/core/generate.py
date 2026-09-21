"""Deterministic current-scope renderer and lock writer."""

from __future__ import annotations

import json
import shutil
import tempfile
from pathlib import Path

from jinja2 import Environment, FileSystemLoader, select_autoescape

from .lock import sha256, write_lock
from .model import expand_board_defaults, load_project, _load_target_manifests
from .rte import resolve_connections
from .validate import assert_valid, validation_report, warning_record


def write_platformio(project_path: str | Path, output_dir: str | Path) -> Path:
    """Write the explicitly best-effort, unpinned PlatformIO adapter."""
    model = assert_valid(project_path)
    target = model["project"]["target"]
    ecu = target["ecu"]
    platform = "espressif32" if ecu == "esp32" else "espressif8266"
    board_name = str(target.get("board", "")).lower()
    boards = {"hw394": "esp32dev", "hw364a": "esp01_1m"}
    board = next((value for key, value in boards.items() if key in board_name), "generic")
    output = Path(output_dir).resolve()
    output.mkdir(parents=True, exist_ok=True)
    path = output / "platformio.ini"
    path.write_text(
        "; Merlin best-effort PlatformIO output; dependencies are intentionally unpinned.\n"
        "; This adapter is outside the reproducibility guarantee.\n"
        "[platformio]\n"
        "default_envs = merlin\n\n"
        "[env:merlin]\n"
        f"platform = {platform}\n"
        f"board = {board}\n"
        "framework = espidf\n"
        "src_dir = main\n",
        encoding="utf-8",
    )
    return path


def _ignore(path: str, names: list[str]) -> set[str]:
    return {name for name in names if name in {"build", "sdkconfig", "project.lock", "__pycache__"}}


def _sdkconfig_values(path: Path) -> dict[str, str]:
    values = {}
    if not path.is_file():
        return values
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("CONFIG_") and "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
        elif line.startswith("# CONFIG_") and line.endswith(" is not set"):
            values[line[2:-11]] = "n"
    return values


def _validate_effective_sdkconfig(source: Path) -> None:
    defaults = _sdkconfig_values(source / "sdkconfig.defaults")
    effective = _sdkconfig_values(source / "sdkconfig")
    if not effective:
        return
    mismatches = [f"{key}={effective.get(key)!r}, expected {value!r}" for key, value in defaults.items() if effective.get(key) != value]
    if mismatches:
        raise ValueError("effective sdkconfig does not honor defaults: " + "; ".join(mismatches))


def _is_golden_project(project: dict, root: Path) -> bool:
    fixture_root = root / "scripts" / "tests" / "fixtures"
    for fixture in sorted(fixture_root.glob("*/project.json")):
        try:
            fixture_project = json.loads(fixture.read_text(encoding="utf-8"))
            fixture_manifests = _load_target_manifests(root, fixture_project)
            fixture_project, _ = expand_board_defaults(fixture_project, fixture_manifests)
            if project == fixture_project:
                return True
        except (OSError, ValueError):
            continue
    return False


def _render_dynamic_metadata(stage: Path, model: dict) -> None:
    project = model["project"]
    main = stage / "code" / "main"
    main.mkdir(parents=True, exist_ok=True)
    (main / "merlin_project.json").write_text(
        json.dumps(project, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    template_root = Path(__file__).resolve().parents[2] / "templates"
    environment = Environment(loader=FileSystemLoader(template_root),
                          autoescape=select_autoescape(), keep_trailing_newline=True)
    rendered = environment.get_template("project_config.c.j2").render(
        name=project["project"]["name"],
        device_count=len(project.get("instances", {}).get("devices", [])),
        swc_count=len(project.get("instances", {}).get("swcs", [])),
    )
    (main / "merlin_project.c").write_text(rendered, encoding="utf-8")
    cmake = main / "CMakeLists.txt"
    if cmake.is_file():
        content = cmake.read_text(encoding="utf-8")
        if '"merlin_project.c"' not in content:
            content = content.replace('SRCS "main.c"', 'SRCS "main.c" "merlin_project.c"')
            cmake.write_text(content, encoding="utf-8")
    root_cmake = stage / "code" / "CMakeLists.txt"
    script = model["root"] / "scripts" / "wizard" / "cli.py"
    if root_cmake.is_file() and script.is_file():
        content = root_cmake.read_text(encoding="utf-8")
        audit = """
if(EXISTS "${CMAKE_SOURCE_DIR}/../project.lock")
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_custom_target(merlin_lock_audit ALL
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/../scripts/wizard/cli.py audit ${CMAKE_SOURCE_DIR}/../project.lock
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/..
        VERBATIM)
endif()
"""
        if "merlin_lock_audit" not in content:
            root_cmake.write_text(content.rstrip() + "\n" + audit, encoding="utf-8")


def _warning_acceptance(model: dict, report: dict, requested: list[str] | None, lock_file: Path) -> list[dict]:
    project = model["project"]
    all_warnings = report["warnings"] + report["acknowledgedWarnings"]
    available = [warning_record(warning) for warning in all_warnings]
    accepted = list(project.get("acknowledgedWarnings", []))
    if lock_file.is_file():
        try:
            lock = json.loads(lock_file.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            lock = {}
        if lock.get("projectHash") == sha256(model["path"]):
            accepted.extend(lock.get("acceptedWarnings", []))
    for value in requested or []:
        rule, separator, object_key = value.partition(":")
        if not separator or not rule or not object_key:
            raise ValueError(f"invalid warning acknowledgement: {value}; use VAL-ID:object")
        matches = [record for record in available if record["id"] == rule and record["object"] == object_key]
        if not matches:
            raise ValueError(f"warning acknowledgement does not match current warnings: {value}")
        accepted.extend(matches)
    unique = {(item.get("id"), item.get("object"), item.get("configHash")): item for item in accepted if isinstance(item, dict)}
    missing = [record for record in available if not any(record == item for item in unique.values())]
    if missing:
        details = ", ".join(f"{item['id']}:{item['object']}" for item in missing)
        raise ValueError(f"unacknowledged warnings: {details}; use --ack VAL-ID:object")
    return [unique[key] for key in sorted(unique)]


def _publish(source: Path, output: Path, model: dict, lock_file: Path, accepted_warnings: list[dict]) -> None:
    """Swap a fully staged tree into place, or leave the previous one intact."""
    parent = output.parent
    parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix=".merlin-stage-", dir=parent))
    backup_root = None
    backup = None
    replaced = False
    try:
        shutil.copytree(source, stage / "code", ignore=_ignore)
        if not _is_golden_project(model["project"], model["root"]):
            _render_dynamic_metadata(stage, model)
        if output.exists():
            backup_root = Path(tempfile.mkdtemp(prefix=".merlin-backup-", dir=parent))
            backup = backup_root / "old"
            output.replace(backup)
        (stage / "code").replace(output)
        replaced = True
        write_lock(model, lock_file, accepted_warnings)
        if backup_root is not None:
            shutil.rmtree(backup_root)
            backup_root = None
    except Exception:
        if replaced and output.exists():
            shutil.rmtree(output)
        if backup is not None and backup.exists():
            backup.replace(output)
        raise
    finally:
        if stage.exists():
            shutil.rmtree(stage)
        if backup_root is not None and backup_root.exists():
            shutil.rmtree(backup_root)


def generate_project(project_path: str | Path, output_dir: str | Path, frozen: bool = False, lock_path: str | Path | None = None, acknowledgements: list[str] | None = None) -> Path:
    model = assert_valid(project_path)
    report = validation_report(project_path)
    if report["errors"]:
        raise ValueError("\n".join(report["errors"]))
    resolve_connections(model["project"], model["root"])
    output = Path(output_dir).resolve()
    root = model["root"]
    reference = model["project"].get("reference")
    if reference is None:
        reference = "v01-reference" if model["project"]["target"]["ecu"] == "esp32" else "v01-hw364a-reference"
    if reference not in {"v01-reference", "v01-hw364a-reference"}:
        raise ValueError("current-scope generator requires an explicit reference tree")
    source = root / reference
    if not source.is_dir():
        raise FileNotFoundError(source)
    _validate_effective_sdkconfig(source)
    lock_file = Path(lock_path).resolve() if lock_path else model["project_dir"] / "project.lock"
    if frozen and not lock_file.exists():
        raise ValueError(f"frozen generation requires an existing lock: {lock_file}")
    if frozen and lock_file.exists():
        from .lock import audit_lock
        drift = audit_lock(lock_file)
        if drift:
            raise ValueError("frozen lock drift:\n" + "\n".join(drift))
    accepted_warnings = _warning_acceptance(model, report, acknowledgements, lock_file)
    _publish(source, output, model, lock_file, accepted_warnings)
    return output
