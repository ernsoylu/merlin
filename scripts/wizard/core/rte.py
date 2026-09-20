"""Provider-agnostic connection checks."""

from __future__ import annotations

import json
from pathlib import Path


def _endpoint(value: str) -> tuple[str, str]:
    instance, port = value.split(".", 1)
    return instance, port


def _manifest_ports(root: Path | None, project: dict) -> tuple[dict, dict]:
    providers = {}
    requires = {}
    if root is None:
        return providers, requires
    for item in project.get("instances", {}).get("devices", []):
        path = root / "drivers" / item["type"] / f"{item['type']}.json"
        if path.is_file():
            manifest = json.loads(path.read_text(encoding="utf-8"))
            for port in manifest.get("provides", []):
                providers[(item["instance"], port["port"])] = dict(port)
    for item in project.get("instances", {}).get("swcs", []):
        name = item["type"].lower()
        path = root / "handcode" / name / f"{name}.json"
        if path.is_file():
            manifest = json.loads(path.read_text(encoding="utf-8"))
            ports = manifest.get("ports", {})
            for port in ports.get("provides", []):
                providers[(item["instance"], port["port"])] = dict(port)
            for port in ports.get("requires", []):
                requires[(item["instance"], port["port"])] = dict(port)
    return providers, requires


def resolve_connections(project: dict, root: str | Path | None = None) -> list[dict]:
    """Resolve the explicit connection list without guessing providers."""
    providers, requires = _manifest_ports(Path(root) if root else None, project)
    for device in project.get("instances", {}).get("devices", []):
        typ = device.get("type")
        if (device["instance"], "Env") not in providers and typ == "bme280":
            providers[(device["instance"], "Env")] = {"interface": "EnvironmentalData", "interfaceVersion": "1.1.0"}
        if (device["instance"], "Health") not in providers and typ == "ssd1306":
            providers[(device["instance"], "Health")] = {"interface": "HealthReport", "interfaceVersion": "1.0.0"}
        if (device["instance"], "Frame") not in providers and typ == "ssd1306":
            providers[(device["instance"], "Frame")] = {"interface": "MonochromeFrame", "interfaceVersion": "1.0.0"}
    for swc in project.get("instances", {}).get("swcs", []):
        typ = swc.get("type")
        if (swc["instance"], "FanOut") not in providers and typ == "ClimateController":
            providers[(swc["instance"], "FanOut")] = {"interface": "PwmDutyCycle", "interfaceVersion": "1.0.0"}
        if (swc["instance"], "DisplayOut") not in providers and typ == "DisplayDemo":
            providers[(swc["instance"], "DisplayOut")] = {"interface": "MonochromeFrame", "interfaceVersion": "1.0.0"}
    resolved = []
    used_targets = set()
    for connection in project.get("connections", []):
        source = _endpoint(connection["from"])
        target = _endpoint(connection["to"])
        provider = providers.get(source)
        if provider is None:
            raise ValueError(f"unresolved provider: {connection['from']}")
        expected = requires.get(target)
        if expected and expected.get("interface") != provider.get("interface"):
            raise ValueError(f"interface mismatch: {connection['from']} -> {connection['to']}")
        if target in used_targets:
            raise ValueError(f"fan-in is not valid: {connection['to']}")
        used_targets.add(target)
        resolved.append({
            "from": connection["from"], "to": connection["to"],
            "interface": provider.get("interface"),
            "version": provider.get("interfaceVersion", ""),
            "requiredVersion": expected.get("interfaceVersion", "") if expected else "",
            "providerElements": provider.get("elements"),
            "acceptedElements": expected.get("acceptedElements", expected.get("elements")) if expected else None,
        })
    connected = {(_endpoint(item["to"])) for item in resolved}
    missing = sorted(set(requires) - connected)
    if missing:
        instance, port = missing[0]
        raise ValueError(f"required port is unbound: {instance}.{port}")
    return resolved
