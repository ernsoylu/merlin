from pathlib import Path
import json

from scripts.wizard.core.model import load_project
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
