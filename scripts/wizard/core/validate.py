"""Schema and semantic validation for current-scope projects."""

from __future__ import annotations

import json
import hashlib
import math
import re
from pathlib import Path

from jsonschema import Draft202012Validator

from .allocate import validate_target_claims
from .model import load_project, resource_claims
from .rte import resolve_connections


def _schema_path(name: str) -> Path:
    return Path(__file__).resolve().parents[2] / "schemas" / f"{name}.json"


def validate_manifest(path: str | Path, kind: str) -> list[str]:
    schema_path = _schema_path(kind)
    if not schema_path.is_file():
        return [f"unknown manifest kind: {kind}"]
    try:
        data = json.loads(Path(path).read_text(encoding="utf-8"))
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        return [str(error)]
    return sorted(error.message for error in Draft202012Validator(schema).iter_errors(data))


def _issue(rule: str, message: str) -> str:
    return f"{rule}: {message}"


def _warning(rule: str, message: str, object_key: str, subtree: object) -> str:
    config_hash = hashlib.sha256(
        json.dumps(subtree, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()
    return f"{rule} [warning] object={object_key} configHash={config_hash}: {message}"


def _load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _driver_path(root: Path, item: dict) -> Path:
    name = str(item.get("type", ""))
    return root / "drivers" / name / f"{name}.json"


def _swc_path(root: Path, item: dict) -> Path:
    name = str(item.get("type", "")).lower()
    return root / "handcode" / name / f"{name}.json"


def _instances(project: dict, kind: str) -> dict[str, dict]:
    return {item.get("instance"): item for item in project.get("instances", {}).get(kind, []) if item.get("instance")}


def _pin_assignments(project: dict) -> list[tuple[str, str, dict]]:
    result = []
    for item in project.get("instances", {}).get("devices", []):
        for pin in item.get("pins", {}).values():
            result.append((str(pin), item.get("instance", "<unnamed>"), item))
        if item.get("pin"):
            result.append((str(item["pin"]), item.get("instance", "<unnamed>"), item))
    for item in project.get("instances", {}).get("iohwab", []):
        if item.get("pin"):
            result.append((str(item["pin"]), item.get("instance", "<unnamed>"), item))
    return result


def _interface_catalog(root: Path) -> dict[str, dict]:
    catalog = {}
    for path in sorted((root / "interfaces").glob("*.json")):
        try:
            interface = _load(path).get("interface", {})
        except (OSError, json.JSONDecodeError):
            continue
        if interface.get("name"):
            catalog[interface["name"]] = interface
    return catalog


def _version(value: str) -> tuple[int, int, int] | None:
    match = re.match(r"^\^?(\d+)\.(\d+)(?:\.(\d+))?", str(value or ""))
    return tuple(int(part or 0) for part in match.groups()) if match else None


def _version_compatible(provider: str, required: str) -> bool:
    actual = _version(provider)
    wanted = _version(required)
    if actual is None or wanted is None:
        return provider == required
    return actual[0] == wanted[0] and actual >= wanted


def _runnable_lookup(root: Path, swcs: dict[str, dict]) -> dict[str, dict]:
    result = {}
    for instance, item in swcs.items():
        path = _swc_path(root, item)
        if not path.is_file():
            continue
        for runnable in _load(path).get("runnables", []):
            result[f"{instance}.{runnable.get('name')}"] = runnable
    return result


def _device_tasks(project: dict) -> dict[str, set[str]]:
    result = {}
    for task, runnables in project.get("runnableMap", {}).items():
        for runnable in runnables:
            instance = str(runnable).split(".", 1)[0]
            result.setdefault(instance, set()).add(task)
    return result


def _val_001(model: dict) -> list[str]:
    conflicts = validate_target_claims(model)
    return [_issue("VAL-001", f"resource {item['resource']} claimed by {item['owners'][0]} and {item['owners'][1]}") for item in conflicts]


def _val_002(model: dict) -> list[str]:
    blocked = set(model["manifests"]["soc"]["data"].get("soc", {}).get("inputOnlyPins", {}).get("pins", []))
    return [_issue("VAL-002", f"{owner} drives input-only pin {pin}") for pin, owner, item in _pin_assignments(model["project"]) if pin in blocked and item.get("direction", "output") in {"output", "pwm"}]


def _val_003(model: dict) -> list[str]:
    strapping = set(model["manifests"]["soc"]["data"].get("soc", {}).get("strappingPins", []))
    return [_warning("VAL-003", f"{owner} uses strapping pin {pin}", owner, item)
            for pin, owner, item in _pin_assignments(model["project"]) if pin in strapping]


def _val_004(model: dict) -> list[str]:
    project = model["project"]
    if not any(project.get("target", {}).get("radio", {}).values()):
        return []
    return [_issue("VAL-004", f"radio-active project claims {claim['resource']}") for claim in resource_claims(model) if claim["resource"] == "ADC2"]


def _val_005(model: dict) -> list[str]:
    seen = set()
    errors = []
    for item in model["project"].get("instances", {}).get("devices", []):
        key = (item.get("bus"), item.get("address"))
        if key[0] and key[1]:
            if key in seen:
                errors.append(_issue("VAL-005", f"duplicate address {key[1]} on {key[0]}"))
            seen.add(key)
    return errors


def _val_006(model: dict) -> list[str]:
    selected = set(model["project"].get("modules", []))
    errors = []
    for item in model["project"].get("instances", {}).get("devices", []):
        path = _driver_path(model["root"], item)
        if not path.is_file():
            continue
        for required in _load(path).get("requiresModules", []):
            if required not in selected:
                errors.append(_issue("VAL-006", f"{item.get('instance')} requires module {required}"))
    return errors


def _val_007(model: dict) -> list[str]:
    try:
        resolve_connections(model["project"], model["root"])
    except (ValueError, KeyError) as error:
        return [_issue("VAL-007", str(error))]
    return []


def _val_008(model: dict) -> list[str]:
    errors = []
    for task in model["project"].get("tasks", []):
        for key in ("periodMs", "deadlineMs", "phaseMs"):
            value = task.get(key)
            if value is not None and (not isinstance(value, int) or (value <= 0 and key != "phaseMs")):
                errors.append(_issue("VAL-008", f"invalid {key} for {task.get('name', '<unnamed>')}"))
        if task.get("deadlineMs", 0) > task.get("periodMs", 0):
            errors.append(_issue("VAL-008", f"deadline exceeds period for {task.get('name', '<unnamed>')}"))
    return errors


def _val_009(model: dict) -> list[str]:
    runnables = _runnable_lookup(model["root"], _instances(model["project"], "swcs"))
    errors = []
    task_by_name = {task.get("name"): task for task in model["project"].get("tasks", [])}
    for task_name, names in model["project"].get("runnableMap", {}).items():
        period = task_by_name.get(task_name, {}).get("periodMs")
        for name in names:
            cycle = runnables.get(name, {}).get("cycleMs")
            if period and cycle and period % cycle:
                errors.append(_issue("VAL-009", f"{name} cycle {cycle}ms is not a divisor of {period}ms"))
    return errors


def _val_010(model: dict) -> list[str]:
    board = model["manifests"]["board"]["data"].get("board", {})
    reservation = board.get("consoleReservation")
    if reservation and model["project"].get("consoleReservation") not in (None, reservation):
        return [_warning("VAL-010", "console UART reservation changed", "consoleReservation", {"board": reservation, "project": model["project"].get("consoleReservation")})]
    return []


def _val_011(model: dict) -> list[str]:
    warnings = []
    for item in model["project"].get("instances", {}).get("swcs", []):
        path = _swc_path(model["root"], item)
        if not path.is_file():
            continue
        for port in _load(path).get("ports", {}).get("requires", []):
            provider_period = port.get("providerAcquisitionPeriodMs")
            if provider_period and port.get("maxAgeMs", 0) < provider_period:
                warnings.append(_warning(
                    "VAL-011", f"{item.get('instance')}.{port.get('port')} maxAgeMs is infeasible",
                    f"{item.get('instance')}.{port.get('port')}", port))
    return warnings


def _val_012(model: dict) -> list[str]:
    runnables = _runnable_lookup(model["root"], _instances(model["project"], "swcs"))
    tasks = {task.get("name"): task for task in model["project"].get("tasks", [])}
    costs = {task: sum(runnables.get(name, {}).get("wcetUs", 0) for name in names) for task, names in model["project"].get("runnableMap", {}).items()}
    warnings = []
    for name, task in tasks.items():
        response = costs.get(name, 0)
        previous = -1
        while response != previous:
            previous = response
            for higher, other in tasks.items():
                if other.get("priority", 0) > task.get("priority", 0) and other.get("periodMs", 0):
                    response = costs.get(name, 0) + math.ceil(response / (other["periodMs"] * 1000)) * costs.get(higher, 0)
        if response > task.get("deadlineMs", 0) * 1000:
            warnings.append(_warning("VAL-012", f"planning response {response}us exceeds {name} deadline", name, task))
    return warnings


def _val_013(model: dict) -> list[str]:
    errors = []
    for kind, item in model["manifests"].items():
        entries = item if isinstance(item, list) else [item]
        for entry in entries:
            errors.extend(_issue("VAL-013", error) for error in validate_manifest(entry["path"], kind.rstrip("s")))
    root = model["root"]
    for directory, kind in (("interfaces", "interface"), ("drivers", "driver"), ("handcode", "swc")):
        for path in sorted(root.glob(f"{directory}/**/*.json")):
            errors.extend(_issue("VAL-013", error) for error in validate_manifest(path, kind))
    return errors


def _val_014(model: dict) -> list[str]:
    errors = []
    for key, version in model["project"].get("manifestVersions", {}).items():
        manifest = model["manifests"].get(key, {}).get("data", {})
        if manifest.get("schemaVersion") != version:
            errors.append(_issue("VAL-014", f"{key} version mismatch"))
    return errors


def _val_015(model: dict) -> list[str]:
    released = set(model["project"].get("releasedResources", []))
    locked = set(model["project"].get("lockedResources", []))
    current = {claim["resource"] for claim in resource_claims(model)}
    return [_issue("VAL-015", f"resource {resource} was reallocated without release") for resource in sorted(locked - released - current)]


def _val_016(model: dict) -> list[str]:
    threshold = model["manifests"]["soc"]["data"].get("soc", {}).get("nativeMuxThresholdHz", 40000000)
    errors = []
    for item in model["project"].get("instances", {}).get("iohwab", []):
        if item.get("frequencyHz", 0) > threshold and item.get("function") and item["function"] not in item.get("nativeMux", []):
            errors.append(_issue("VAL-016", f"{item.get('instance')} lacks native mux for {item.get('function')}"))
    return errors


def _val_017(model: dict) -> list[str]:
    warnings = []
    project = model["project"]
    for bus, config in project.get("buses", {}).items():
        paths = [_driver_path(model["root"], item) for item in project.get("instances", {}).get("devices", []) if item.get("bus") == bus]
        limits = [_load(path).get("bus", {}).get("maxSpeedHz") for path in paths if path.is_file() and _load(path).get("bus", {}).get("maxSpeedHz")]
        if limits and config.get("speedHz", 0) > min(limits):
            warnings.append(_warning("VAL-017", f"{bus} is clamped to {min(limits)}Hz", bus, config))
    return warnings


def _val_018(model: dict) -> list[str]:
    errors = []
    board = model["manifests"]["board"]["data"].get("board", {})
    for name, bus in model["project"].get("buses", {}).items():
        speed = bus.get("speedHz", 0)
        pullups = bus.get("pullups") or board.get("buses", {}).get(name, {}).get("pullups", {})
        if speed > 100000 and pullups.get("present") is not True:
            errors.append(_issue("VAL-018", f"{name} above 100kHz requires declared external pull-ups"))
        resistance = pullups.get("valueOhms")
        capacitance = bus.get("capacitancePf")
        limit = 300 if speed > 100000 else 1000
        if resistance and capacitance and 0.847 * resistance * capacitance * 1e-3 > limit:
            errors.append(_issue("VAL-018", f"{name} rise time exceeds {limit}ns"))
    return errors


def _val_019(model: dict) -> list[str]:
    tasks = _device_tasks(model["project"])
    by_bus = {}
    for item in model["project"].get("instances", {}).get("devices", []):
        if item.get("bus"):
            by_bus.setdefault(item["bus"], []).append(item["instance"])
    errors = []
    for bus, devices in by_bus.items():
        assigned = {task for device in devices for task in tasks.get(device, set())}
        if len(assigned) > 1:
            errors.append(_issue("VAL-019", f"devices on {bus} are mapped to different tasks"))
    return errors


def _val_020(model: dict) -> list[str]:
    try:
        resolve_connections(model["project"], model["root"])
    except (ValueError, KeyError) as error:
        return [_issue("VAL-020", str(error))]
    return []


def _val_021(model: dict) -> list[str]:
    budget = model["project"].get("memoryBudgetBytes")
    used = sum(item.get("resources", {}).get("ramBytes", 0) for item in model["project"].get("instances", {}).get("swcs", []))
    return [_warning("VAL-021", f"declared RAM budget {budget}B is below {used}B", "project.memoryBudgetBytes", {"budget": budget, "used": used})] if budget is not None and used > budget else []


def _val_022(model: dict) -> list[str]:
    target = model["project"].get("target", {})
    if target.get("crypto") and not any(target.get("radio", {}).values()) and not target.get("rng"):
        return [_issue("VAL-022", "crypto requires radio or an explicit RNG")]
    return []


def _val_023(model: dict) -> list[str]:
    errors = []
    for item in model["project"].get("instances", {}).get("devices", []):
        path = _driver_path(model["root"], item)
        if not path.is_file():
            continue
        contract = _load(path).get("executionContract", {})
        timeout = contract.get("transactionTimeoutMs")
        recovery = contract.get("recovery", {}).get("budgetUs", 0)
        elapsed = contract.get("maxElapsedMs")
        if timeout is not None and elapsed is not None and elapsed * 1000 < timeout * 1000 + recovery:
            errors.append(_issue("VAL-023", f"{item.get('type')} maxElapsedMs does not cover timeout and recovery"))
    return errors


def _val_024(model: dict) -> list[str]:
    catalog = _interface_catalog(model["root"])
    try:
        resolved = resolve_connections(model["project"], model["root"])
    except (ValueError, KeyError):
        return []
    return [_issue("VAL-024", f"interface version mismatch for {item['interface']}") for item in resolved if item["interface"] in catalog and item.get("requiredVersion") and not _version_compatible(item.get("version", ""), item.get("requiredVersion", ""))]


def _val_025(model: dict) -> list[str]:
    catalog = _interface_catalog(model["root"])
    try:
        resolved = resolve_connections(model["project"], model["root"])
    except (ValueError, KeyError):
        return []
    errors = [_issue("VAL-025", f"unknown interface {item['interface']}") for item in resolved if item["interface"] not in catalog]
    for item in resolved:
        interface = catalog.get(item["interface"])
        if interface is None:
            continue
        provider_elements = {element.get("name"): element for element in (item.get("providerElements") or interface.get("elements", []))}
        accepted_elements = {element.get("name"): element for element in (item.get("acceptedElements") or interface.get("elements", []))}
        for name, provider in provider_elements.items():
            accepted = accepted_elements.get(name)
            if accepted is None:
                errors.append(_issue("VAL-025", f"provider element {name} is not accepted by {item['to']}"))
                continue
            if provider.get("type") != accepted.get("type"):
                errors.append(_issue("VAL-025", f"element type mismatch for {item['interface']}.{name}"))
            if provider.get("unit", accepted.get("unit")) != accepted.get("unit", provider.get("unit")):
                errors.append(_issue("VAL-025", f"element unit mismatch for {item['interface']}.{name}"))
            source_range = provider.get("range")
            accepted_range = accepted.get("acceptedRange", accepted.get("range"))
            if source_range and accepted_range and (source_range[0] < accepted_range[0] or source_range[1] > accepted_range[1]):
                errors.append(_issue("VAL-025", f"provider range exceeds consumer range for {item['interface']}.{name}"))
    return errors


def _val_026(model: dict) -> list[str]:
    project = model["project"]
    target = project.get("target", {})
    errors = []
    expected_sdk = {"esp32": ("esp-idf", "5.2.3"), "esp8266": ("esp8266-rtos-sdk", "3.4")}.get(target.get("ecu"))
    if expected_sdk and (target.get("sdk", {}).get("family"), target.get("sdk", {}).get("version")) != expected_sdk:
        errors.append(_issue("VAL-026", f"SDK does not match {target.get('ecu')} backend"))
    board = model["manifests"]["board"]["data"].get("board", {})
    if target.get("ecu") != board.get("ecu"):
        errors.append(_issue("VAL-026", "target ECU does not match board ECU"))
    if target.get("ecu") == "esp8266" and target.get("radio", {}).get("bt"):
        errors.append(_issue("VAL-026", "ESP8266 Bluetooth is unsupported"))
    if target.get("radio", {}).get("bt") and "Bluetooth" in set(board.get("unsupported", [])):
        errors.append(_issue("VAL-026", "board does not support Bluetooth"))
    for item in project.get("instances", {}).get("devices", []):
        path = _driver_path(model["root"], item)
        if path.is_file() and _load(path).get("driverType", {}).get("qualification") in {"deferred", "unverified", "unsupported"} and item.get("provider") != "simulation":
            errors.append(_issue("VAL-026", f"driver {item.get('type')} is not selectable"))
    return errors


_ERROR_RULES = (_val_001, _val_002, _val_004, _val_005, _val_006, _val_007, _val_008, _val_009, _val_013, _val_014, _val_015, _val_016, _val_018, _val_019, _val_020, _val_022, _val_023, _val_024, _val_025, _val_026)
_WARNING_RULES = (_val_003, _val_010, _val_011, _val_012, _val_017, _val_021)
_WARNING_RE = re.compile(r"^(VAL-\d{3}) \[warning\] object=(\S+) configHash=([0-9a-f]+):")
VALIDATION_RULES = {f"VAL-{number:03d}": rule for number, rule in (
    [(1, _val_001), (2, _val_002), (3, _val_003), (4, _val_004),
     (5, _val_005), (6, _val_006), (7, _val_007), (8, _val_008),
     (9, _val_009), (10, _val_010), (11, _val_011), (12, _val_012),
     (13, _val_013), (14, _val_014), (15, _val_015), (16, _val_016),
     (17, _val_017), (18, _val_018), (19, _val_019), (20, _val_020),
     (21, _val_021), (22, _val_022), (23, _val_023), (24, _val_024),
     (25, _val_025), (26, _val_026)]
)}


def validation_report(path: str | Path) -> dict[str, list[str]]:
    try:
        model = load_project(path)
    except (OSError, json.JSONDecodeError, KeyError, TypeError) as error:
        return {"errors": [f"invalid project: {error}"], "warnings": []}
    project = model["project"]
    schema = json.loads(_schema_path("project").read_text(encoding="utf-8"))
    errors = [error.message for error in Draft202012Validator(schema).iter_errors(project)]
    errors.extend(model["conflicts"])
    errors.extend(error for rule in _ERROR_RULES for error in rule(model))
    raw_warnings = sorted(set(warning for rule in _WARNING_RULES for warning in rule(model)))
    accepted = project.get("acknowledgedWarnings", [])
    warnings = [warning for warning in raw_warnings if not _warning_acknowledged(warning, accepted)]
    return {"errors": sorted(set(errors)), "warnings": warnings, "acknowledgedWarnings": [warning for warning in raw_warnings if warning not in warnings]}


def warning_record(warning: str) -> dict[str, str]:
    match = _WARNING_RE.match(warning)
    if not match:
        raise ValueError(f"invalid warning: {warning}")
    rule, object_key, config_hash = match.groups()
    return {"id": rule, "object": object_key, "configHash": config_hash}


def _warning_acknowledged(warning: str, accepted: list[dict]) -> bool:
    try:
        record = warning_record(warning)
    except ValueError:
        return False
    return any(
        isinstance(item, dict)
        and item.get("id", item.get("rule")) == record["id"]
        and item.get("object") == record["object"]
        and item.get("configHash") == record["configHash"]
        for item in accepted
    )


def validate_project(path: str | Path) -> list[str]:
    return validation_report(path)["errors"]


def assert_valid(path: str | Path) -> dict:
    report = validation_report(path)
    if report["errors"]:
        raise ValueError("\n".join(report["errors"]))
    return load_project(path)
