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


def make_lock(model: dict, lock_root: Path | None = None, accepted_warnings: list[dict] | None = None) -> dict:
    project_path = model["path"]
    lock_root = lock_root or model["project_dir"]
    sources = {os.path.relpath(project_path, lock_root): sha256(project_path)}
    for item in model["manifests"].values():
        if isinstance(item, list):
            for entry in item:
                path = Path(entry["path"])
                sources[os.path.relpath(path, lock_root)] = sha256(path)
        else:
            path = Path(item["path"])
            sources[os.path.relpath(path, lock_root)] = sha256(path)
    for path in sorted(model["root"].glob("interfaces/*.json")):
        sources[os.path.relpath(path, lock_root)] = sha256(path)
    for path in sorted(model["root"].glob("scripts/schemas/*.json")):
        sources[os.path.relpath(path, lock_root)] = sha256(path)
    for path in sorted(model["root"].glob("scripts/templates/*")):
        if path.is_file():
            sources[os.path.relpath(path, lock_root)] = sha256(path)
    project = model["project"]
    for item in project.get("instances", {}).get("devices", []):
        path = model["root"] / "drivers" / item["type"] / f"{item['type']}.json"
        if path.is_file():
            sources[os.path.relpath(path, lock_root)] = sha256(path)
    for item in project.get("instances", {}).get("swcs", []):
        path = model["root"] / "handcode" / item["type"].lower() / f"{item['type'].lower()}.json"
        if path.is_file():
            sources[os.path.relpath(path, lock_root)] = sha256(path)
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
