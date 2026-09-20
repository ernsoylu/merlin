import copy
import json
import shutil
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from scripts.wizard.core.generate import generate_project  # noqa: E402
from scripts.wizard.core.rte import resolve_connections  # noqa: E402
from scripts.wizard.core.validate import validation_report  # noqa: E402


ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "scripts/tests/fixtures/00-climate-demo/project.json"


def _project(tmp_path, mutate=None):
    data = json.loads(FIXTURE.read_text())
    if mutate:
        mutate(data)
    path = tmp_path / "project.json"
    path.write_text(json.dumps(data))
    return path


def test_negative_current_scope_cases_are_rejected(tmp_path):
    cases = [
        lambda data: data["target"]["sdk"].update(version="9.9.9"),
        lambda data: data["modules"].remove("I2c"),
        lambda data: data["instances"]["devices"][1].update(address="0x76"),
        lambda data: data["instances"]["devices"][0].update(provider="physical"),
        lambda data: data["target"].update(radio={"wifi": False, "bt": True}, ecu="esp8266"),
    ]
    expected = ("VAL-026", "VAL-006", "VAL-005", "VAL-026", "VAL-026")
    for mutate, rule in zip(cases, expected):
        report = validation_report(_project(tmp_path, mutate))
        assert any(item.startswith(rule) for item in report["errors"]), report


def test_provider_swap_keeps_typed_rte_binding(tmp_path):
    shutil.copytree(ROOT / "handcode", tmp_path / "handcode")
    shutil.copytree(ROOT / "interfaces", tmp_path / "interfaces")
    shutil.copytree(ROOT / "drivers", tmp_path / "drivers")
    alternate = tmp_path / "drivers" / "simalt"
    alternate.mkdir()
    manifest = json.loads((ROOT / "drivers/bme280/bme280.json").read_text())
    manifest["driverType"]["name"] = "simalt"
    (alternate / "simalt.json").write_text(json.dumps(manifest))
    project = json.loads(FIXTURE.read_text())
    project["instances"]["devices"][0]["type"] = "simalt"
    original = resolve_connections(json.loads(FIXTURE.read_text()), ROOT)
    swapped = resolve_connections(project, tmp_path)
    assert [(item["interface"], item["version"]) for item in swapped] == [(item["interface"], item["version"]) for item in original]


def test_rte_rejects_version_and_structural_contract_mismatch(tmp_path):
    for directory in ("devkits", "drivers", "handcode", "interfaces", "modules", "soc"):
        shutil.copytree(ROOT / directory, tmp_path / directory)
    data = json.loads(FIXTURE.read_text())
    data["target"]["soc"] = "soc/esp32.json"
    data["target"]["module"] = "modules/esp32-wroom-32.json"
    data["target"]["board"] = "devkits/devkit-hw394.json"
    swc = tmp_path / "handcode/climatecontroller/climatecontroller.json"
    manifest = json.loads(swc.read_text())
    manifest["ports"]["requires"][0]["interfaceVersion"] = "^2.0.0"
    manifest["ports"]["requires"][0]["acceptedElements"] = [{"name": "Temperature", "type": "uint16", "unit": "degC", "acceptedRange": [0, 40]}]
    swc.write_text(json.dumps(manifest))
    project = tmp_path / "project.json"
    project.write_text(json.dumps(data))
    report = validation_report(project)
    assert any(item.startswith("VAL-024") for item in report["errors"])
    assert any(item.startswith("VAL-025") for item in report["errors"])


def test_generator_capacity_fixture_is_under_nfr(tmp_path):
    def make_large(data):
        data["instances"]["devices"] = [{"instance": f"dio{i:02d}", "type": "dio"} for i in range(50)]
        data["instances"]["swcs"] = [{"instance": f"swc{i:02d}", "type": "DisplayDemo"} for i in range(30)]
        data["tasks"] = [{"name": f"T{i}", "periodMs": 100, "deadlineMs": 50, "priority": 5, "core": 0} for i in range(8)]
        data["runnableMap"] = {}
        data["connections"] = []

    project = _project(tmp_path, make_large)
    started = time.monotonic()
    generate_project(project, tmp_path / "code", lock_path=tmp_path / "project.lock")
    assert time.monotonic() - started < 30
