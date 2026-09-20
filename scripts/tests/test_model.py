from pathlib import Path
import json

from scripts.wizard.core.model import load_project, resource_claims
from scripts.wizard.core.allocate import validate_target_claims


ROOT = Path(__file__).resolve().parents[2]


def test_hw364a_board_default_is_idempotent():
    model = load_project(ROOT / "scripts/tests/fixtures/01-hw364a-oled-demo/project.json")
    devices = model["project"]["instances"]["devices"]
    assert [item["instance"] for item in devices] == ["onboardOled"]
    assert devices[0]["pins"] == {"sda": "GPIO14", "scl": "GPIO12"}


def test_generic_esp8266_has_no_implicit_oled():
    path = ROOT / "scripts/tests/fixtures/01-hw364a-oled-demo/project.json"
    data = json.loads(path.read_text())
    data["target"]["board"] = "devkits/generic-esp8266.json"
    data["instances"]["devices"] = []
    temporary = ROOT / "scripts/tests/fixtures/01-hw364a-oled-demo/generic-project.json"
    temporary.write_text(json.dumps(data))
    try:
        model = load_project(temporary)
        assert model["project"]["instances"]["devices"] == []
        assert validate_target_claims(model) == []
    finally:
        temporary.unlink()


def test_overlay_claim_is_added_to_allocation(tmp_path):
    overlay = tmp_path / "overlay.json"
    overlay.write_text(json.dumps({
        "schemaVersion": "2.2.0",
        "overlay": {"name": "lab-header", "claims": [{"resource": "IO4"}]},
    }))
    data = json.loads((ROOT / "scripts/tests/fixtures/00-climate-demo/project.json").read_text())
    data["target"]["overlays"] = [str(overlay)]
    project = tmp_path / "project.json"
    project.write_text(json.dumps(data))

    model = load_project(project)
    assert model["project"]["target"]["overlays"] == [str(overlay)]
    assert {"resource": "IO4", "owner": "overlay:lab-header"} in resource_claims(model)
