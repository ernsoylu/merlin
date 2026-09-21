"""Provider-agnostic connection checks."""

from __future__ import annotations

import json
from pathlib import Path


def _endpoint(value: str) -> tuple[str, str]:
    instance, port = value.split(".", 1)
    return instance, port


def _manifest(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8")) if path.is_file() else {}


def _manifest_ports(root: Path | None, project: dict) -> tuple[dict, dict]:
    providers = {}
    requires = {}
    if root is None:
        return providers, requires
    for item in project.get("instances", {}).get("devices", []):
        manifest = _manifest(root / "drivers" / item["type"] / f"{item['type']}.json")
        for port in manifest.get("provides", []):
            providers[(item["instance"], port["port"])] = dict(port)
    for item in project.get("instances", {}).get("swcs", []):
        name = item["type"].lower()
        ports = _manifest(root / "handcode" / name / f"{name}.json").get("ports", {})
        for port in ports.get("provides", []):
            providers[(item["instance"], port["port"])] = dict(port)
        for port in ports.get("requires", []):
            requires[(item["instance"], port["port"])] = dict(port)
    return providers, requires


# Providers the current-scope reference types supply without a manifest entry.
_IMPLICIT_PROVIDERS = {
    ("devices", "bme280", "Env"): {"interface": "EnvironmentalData", "interfaceVersion": "1.1.0"},
    ("devices", "ssd1306", "Health"): {"interface": "HealthReport", "interfaceVersion": "1.0.0"},
    ("devices", "ssd1306", "Frame"): {"interface": "MonochromeFrame", "interfaceVersion": "1.0.0"},
    ("swcs", "ClimateController", "FanOut"): {"interface": "PwmDutyCycle", "interfaceVersion": "1.0.0"},
    ("swcs", "DisplayDemo", "DisplayOut"): {"interface": "MonochromeFrame", "interfaceVersion": "1.0.0"},
}


def _add_implicit_providers(project: dict, providers: dict) -> None:
    for (kind, kind_type, port), definition in _IMPLICIT_PROVIDERS.items():
        for item in project.get("instances", {}).get(kind, []):
            if item.get("type") == kind_type:
                providers.setdefault((item["instance"], port), dict(definition))


def resolve_connections(project: dict, root: str | Path | None = None) -> list[dict]:
    """Resolve the explicit connection list without guessing providers."""
    providers, requires = _manifest_ports(Path(root) if root else None, project)
    _add_implicit_providers(project, providers)
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
