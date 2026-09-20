"""Load and resolve the small, deterministic current-scope project model."""

from __future__ import annotations

import copy
import json
from pathlib import Path


SCHEMA_VERSION = "2.2.0"


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def resolve_ref(root: Path, reference: str) -> Path:
    path = (root / reference).resolve()
    if not path.is_file():
        raise FileNotFoundError(f"manifest not found: {reference}")
    return path


def _load_target_manifests(root: Path, project: dict) -> dict[str, dict]:
    target = project["target"]
    result = {}
    for key in ("soc", "module", "board"):
        path = resolve_ref(root, target[key])
        result[key] = {"path": str(path), "data": read_json(path)}
    for reference in target.get("overlays", []):
        path = resolve_ref(root, reference)
        result.setdefault("overlays", []).append({"path": str(path), "data": read_json(path)})
    return result


def expand_board_defaults(project: dict, manifests: dict[str, dict]) -> tuple[dict, list[str]]:
    """Expand fixed board devices exactly once and report conflicts."""
    result = copy.deepcopy(project)
    devices = result.setdefault("instances", {}).setdefault("devices", [])
    by_name = {item["instance"]: item for item in devices}
    board = manifests["board"]["data"].get("board", {})
    conflicts = []
    for default in board.get("onboardDevices", []):
        name = default["instance"]
        existing = by_name.get(name)
        if existing is None:
            devices.append(copy.deepcopy(default))
            by_name[name] = devices[-1]
        else:
            for key, value in default.items():
                if key in existing and existing[key] != value:
                    conflicts.append(f"board default conflicts with instances.devices[{name}].{key}")
                else:
                    existing.setdefault(key, copy.deepcopy(value))
    devices.sort(key=lambda item: item["instance"])
    return result, conflicts


def load_project(path: str | Path) -> dict:
    project_path = Path(path).resolve()
    project = read_json(project_path)
    root = project_path.parent
    for candidate in (project_path.parent, *project_path.parents):
        if (candidate / project["target"]["soc"]).is_file():
            root = candidate
            break
    else:
        repository = Path(__file__).resolve().parents[3]
        if (repository / project["target"]["soc"]).is_file():
            root = repository
    manifests = _load_target_manifests(root, project)
    expanded, conflicts = expand_board_defaults(project, manifests)
    return {
        "path": project_path,
        "project_dir": project_path.parent,
        "root": root,
        "project": expanded,
        "manifests": manifests,
        "conflicts": conflicts,
    }


def resource_claims(model: dict) -> list[dict]:
    project = model["project"]
    claims = []
    board = model["manifests"]["board"]["data"].get("board", {})
    fixed = board.get("fixedResourcesVerified", board.get("name") == "HW-364A")
    board_names = {device.get("instance") for device in board.get("onboardDevices", [])}
    if fixed:
        for device in board.get("onboardDevices", []):
            for pin in device.get("pins", {}).values():
                claim = {"resource": pin, "owner": device["instance"]}
                if device.get("bus"):
                    claim["share_key"] = f"bus-pin:{device['bus']}"
                claims.append(claim)
            if "bus" in device:
                claims.append({"resource": device["bus"], "owner": device["instance"], "share_key": "i2c-master"})
    for bus, config in project.get("buses", {}).items():
        for pin in config.get("pins", {}).values():
            claims.append({"resource": pin, "owner": bus, "share_key": f"bus-pin:{bus}"})
        claims.append({"resource": bus, "owner": bus, "share_key": "i2c-master"})
    for device in project.get("instances", {}).get("devices", []):
        if not fixed and device.get("instance") in board_names:
            continue
        bus = device.get("bus")
        if bus:
            claims.append({"resource": bus, "owner": device["instance"], "share_key": "i2c-master"})
        if device.get("address"):
            claims.append({"resource": f"{bus}:{device['address']}", "owner": device["instance"]})
        for pin in device.get("pins", {}).values():
            claim = {"resource": pin, "owner": device["instance"]}
            if bus:
                claim["share_key"] = f"bus-pin:{bus}"
            claims.append(claim)
        if device.get("pin"):
            claims.append({"resource": device["pin"], "owner": device["instance"]})
    for item in project.get("instances", {}).get("iohwab", []):
        if item.get("pin"):
            claims.append({"resource": item["pin"], "owner": item["instance"]})
        if item.get("timer") is not None:
            claims.append({"resource": f"PWM_TIMER{item['timer']}", "owner": item["instance"], "share_key": f"pwm:{item.get('frequencyHz')}:{item.get('resolutionBits')}"})
        if item.get("channel") is not None:
            claims.append({"resource": f"PWM_CHANNEL{item['channel']}", "owner": item["instance"]})
    if project.get("target", {}).get("radio", {}).get("wifi"):
        claims.append({"resource": "WLAN", "owner": "radio"})
    for overlay in model["manifests"].get("overlays", []):
        definition = overlay["data"].get("overlay", {})
        default_owner = f"overlay:{definition.get('name', '<unnamed>')}"
        for item in definition.get("claims", []):
            if not isinstance(item, dict) or not item.get("resource"):
                continue
            claim = {"resource": item["resource"], "owner": item.get("owner", default_owner)}
            if item.get("share_key"):
                claim["share_key"] = item["share_key"]
            claims.append(claim)
    return claims
