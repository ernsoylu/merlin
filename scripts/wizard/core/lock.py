"""Deterministic lock and source-drift checks."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

from jsonschema import Draft202012Validator

from .model import resource_claims


TEMPLATE_VERSION = "0.1.0"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


_GLOBBED_SOURCES = ("interfaces/*.json", "scripts/schemas/*.json", "scripts/templates/*")


def _add_source(sources: dict, path: Path, lock_root: Path) -> None:
    sources[os.path.relpath(path, lock_root)] = sha256(path)


def _manifest_sources(model: dict, lock_root: Path, sources: dict) -> None:
    for item in model["manifests"].values():
        for entry in item if isinstance(item, list) else [item]:
            _add_source(sources, Path(entry["path"]), lock_root)


def _glob_sources(model: dict, lock_root: Path, sources: dict) -> None:
    for pattern in _GLOBBED_SOURCES:
        for path in sorted(model["root"].glob(pattern)):
            if path.is_file():
                _add_source(sources, path, lock_root)


def _type_sources(model: dict, lock_root: Path, sources: dict) -> None:
    instances = model["project"].get("instances", {})
    types = [(item["type"], "drivers") for item in instances.get("devices", [])]
    types += [(item["type"].lower(), "handcode") for item in instances.get("swcs", [])]
    for name, folder in types:
        path = model["root"] / folder / name / f"{name}.json"
        if path.is_file():
            _add_source(sources, path, lock_root)


def make_lock(model: dict, lock_root: Path | None = None, accepted_warnings: list[dict] | None = None) -> dict:
    project_path = model["path"]
    lock_root = lock_root or model["project_dir"]
    project = model["project"]
    sources = {os.path.relpath(project_path, lock_root): sha256(project_path)}
    _manifest_sources(model, lock_root, sources)
    _glob_sources(model, lock_root, sources)
    _type_sources(model, lock_root, sources)
    return {
        "schemaVersion": "2.2.0",
        "generatorVersion": model["project"].get("generatorVersion", "0.1.0"),
        "templateVersion": TEMPLATE_VERSION,
        "projectHash": sha256(project_path),
        "target": model["project"]["target"],
        "sources": dict(sorted(sources.items())),
        "resolved": {
            "devices": project.get("instances", {}).get("devices", []),
            "swcs": project.get("instances", {}).get("swcs", []),
            "buses": project.get("buses", {}),
            "connections": project.get("connections", []),
            "resources": resource_claims(model),
        },
        "acceptedWarnings": accepted_warnings if accepted_warnings is not None else project.get("acknowledgedWarnings", []),
    }


def write_lock(model: dict, path: Path, accepted_warnings: list[dict] | None = None) -> None:
    path.write_text(json.dumps(make_lock(model, path.parent, accepted_warnings), indent=2, sort_keys=True) + "\n", encoding="utf-8")


def audit_lock(path: str | Path) -> list[str]:
    lock_path = Path(path).resolve()
    try:
        lock = json.loads(lock_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        return [f"invalid lock: {error}"]
    errors = []
    schema_path = Path(__file__).resolve().parents[2] / "schemas" / "lock.json"
    if schema_path.is_file():
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
        errors.extend(f"lock schema: {error.message}" for error in Draft202012Validator(schema).iter_errors(lock))
    required = {"schemaVersion", "generatorVersion", "projectHash", "target", "sources"}
    errors.extend(f"missing lock field: {field}" for field in sorted(required - set(lock)))
    for name, expected in lock.get("sources", {}).items():
        source = lock_path.parent / name
        if not source.is_file():
            errors.append(f"missing source: {name}")
        elif sha256(source) != expected:
            errors.append(f"source drift: {name}")
    return errors
