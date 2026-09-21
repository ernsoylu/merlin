from __future__ import annotations

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
REQ = re.compile(r"\bREQ-[A-Z]+-\d{3}\b")
SCHEMAS = {
    "board.json",
    "driver.json",
    "interface.json",
    "lock.json",
    "module.json",
    "overlay.json",
    "project.json",
    "soc.json",
    "swc.json",
}


def _requirement_ids(path: Path) -> set[str]:
    return set(REQ.findall(path.read_text(encoding="utf-8")))


def test_design_requirements_are_traceable() -> None:
    design = _requirement_ids(ROOT / "PROJECT_DEFINITION.md")
    matrix = _requirement_ids(ROOT / "docs/traceability.md")
    assert design <= matrix, sorted(design - matrix)


def test_traceability_rows_are_unique_and_structured() -> None:
    rows = [
        line
        for line in (ROOT / "docs/traceability.md").read_text(encoding="utf-8").splitlines()
        if line.startswith("| REQ-")
    ]
    ids = [line.split("|", 2)[1].strip() for line in rows]
    assert rows
    assert len(ids) == len(set(ids))
    assert all(len(line.split("|")) >= 6 for line in rows)


def test_schema_catalog_is_frozen_software_side() -> None:
    paths = {path.name for path in (ROOT / "scripts/schemas").glob("*.json")}
    assert paths == SCHEMAS
    for path in sorted((ROOT / "scripts/schemas").glob("*.json")):
        schema = json.loads(path.read_text(encoding="utf-8"))
        assert schema["properties"]["schemaVersion"]["const"] == "2.2.0"
