import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from scripts.wizard.core.validate import VALIDATION_RULES, validation_report  # noqa: E402


ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "scripts/tests/fixtures/00-climate-demo/project.json"


def test_all_declared_validation_rules_have_a_callable():
    assert set(VALIDATION_RULES) == {f"VAL-{number:03d}" for number in range(1, 27)}
    assert all(callable(rule) for rule in VALIDATION_RULES.values())


def test_current_fixture_has_no_errors_and_reports_only_review_warnings():
    report = validation_report(FIXTURE)
    assert report["errors"] == []
    assert all(item.startswith("VAL-") for item in report["warnings"])


def test_duplicate_address_is_an_error(tmp_path):
    data = json.loads(FIXTURE.read_text())
    data["instances"]["devices"][1]["address"] = data["instances"]["devices"][0]["address"]
    project = tmp_path / "project.json"
    project.write_text(json.dumps(data))
    report = validation_report(project)
    assert any(item.startswith("VAL-005") for item in report["errors"])


def test_console_reservation_change_is_review_warning(tmp_path):
    data = json.loads(FIXTURE.read_text())
    data.pop("acknowledgedWarnings", None)
    data["consoleReservation"] = "UART1"
    project = tmp_path / "project.json"
    project.write_text(json.dumps(data))
    report = validation_report(project)
    assert report["errors"] == []
    assert any(item.startswith("VAL-010 [warning]") for item in report["warnings"])


def test_reallocation_requires_explicit_release(tmp_path):
    data = json.loads(FIXTURE.read_text())
    data["lockedResources"] = ["IO99"]
    project = tmp_path / "project.json"
    project.write_text(json.dumps(data))
    report = validation_report(project)
    assert any(item.startswith("VAL-015") for item in report["errors"])
    data["releasedResources"] = ["IO99"]
    project.write_text(json.dumps(data))
    assert not any(item.startswith("VAL-015") for item in validation_report(project)["errors"])


def test_deferred_external_driver_is_not_selectable(tmp_path):
    data = json.loads(FIXTURE.read_text())
    data["instances"]["devices"][0]["provider"] = "physical"
    project = tmp_path / "project.json"
    project.write_text(json.dumps(data))
    report = validation_report(project)
    assert any(item.startswith("VAL-026") for item in report["errors"])


def test_warning_acknowledgement_is_bound_to_object_hash(tmp_path):
    data = json.loads(FIXTURE.read_text())
    data.pop("acknowledgedWarnings", None)
    initial = tmp_path / "initial.json"
    initial.write_text(json.dumps(data))
    warning = validation_report(initial)["warnings"][0]
    match = re.match(r"^(VAL-\d{3}) \[warning\] object=(\S+) configHash=([0-9a-f]{64}):", warning)
    assert match
    data["acknowledgedWarnings"] = [{"id": match[1], "object": match[2], "configHash": match[3]}]
    acknowledged = tmp_path / "acknowledged.json"
    acknowledged.write_text(json.dumps(data))
    report = validation_report(acknowledged)
    assert warning not in report["warnings"]
    data["instances"]["iohwab"][0]["frequencyHz"] += 1
    changed = tmp_path / "changed.json"
    changed.write_text(json.dumps(data))
    assert any(item.startswith("VAL-003") for item in validation_report(changed)["warnings"])
