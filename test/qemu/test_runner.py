import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from run_smoke import assert_scenario  # noqa: E402
from run_scenarios import _scenario_defaults  # noqa: E402


def _records(items):
    return "\n".join(json.dumps(item) for item in items)


def test_scenario_assertions_cover_non_physical_fault_paths():
    normal = [{"instance": name, "health": "READY", "sequence": index, "temperatureCentiDegC": 1} for index in range(1, 4) for name in ("ambientSensor", "enclosureSensor")]
    assert assert_scenario(_records(normal), "normal") == 6
    recovery = normal + [{"instance": "enclosureSensor", "health": "READY", "sequence": 4, "temperatureCentiDegC": 1}]
    assert assert_scenario(_records(recovery), "fake-disconnect-recovery") == 7
    stale = [item for item in normal if item["instance"] == "ambientSensor"] + [{"instance": "enclosureSensor", "health": "READY", "sequence": index, "temperatureCentiDegC": 1} for index in range(3, 14)]
    assert assert_scenario(_records(stale), "stale-failsafe") == 14
    assert assert_scenario(_records([{"fault": "RTF-002", "deadlineFaults": 1}]), "deadline-skip") == 1
    assert assert_scenario(_records([{"system": "SAFE_HALT", "reason": "BOOT_LOOP"}]), "controlled-reset-safe-halt") == 1


def test_scenario_defaults_are_valid_kconfig_lines():
    assert all(item.startswith("CONFIG_") for item in _scenario_defaults("normal"))
