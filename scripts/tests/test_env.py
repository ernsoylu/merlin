"""Tests for toolchain pin loading and version parsing.

These matter because check-env's whole job is being diagnosable: a banner it
misparses becomes a confident wrong answer about the toolchain.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from scripts.wizard import cli  # noqa: E402
from scripts.wizard.core import env  # noqa: E402
from scripts.wizard.core.validate import validate_manifest  # noqa: E402


def test_toolchain_env_parses_and_pins_idf():
    pins = env.load_toolchain()
    assert pins["ESP_IDF_VERSION"] == "5.2.3"
    assert pins["PYTHON_MIN"] == "3.10"
    assert "#" not in "".join(pins)


def test_toolchain_env_ignores_comments_and_blanks(tmp_path):
    path = tmp_path / "toolchain.env"
    path.write_text("# a comment\n\nFOO=1\n  BAR = two  \n")
    assert env.load_toolchain(path) == {"FOO": "1", "BAR": "two"}


def test_parse_version_handles_real_banners():
    assert env.parse_version("ESP-IDF v5.2.3") == "5.2.3"
    assert env.parse_version("ESP-IDF v5.2.3-dirty") == "5.2.3"
    assert env.parse_version("qemu-system-xtensa version 8.2.0 (v8.2.0-x)") == "8.2.0"
    assert env.parse_version("xtensa-esp32-elf-gcc (crosstool-NG) 13.2.0") == "13.2.0"
    assert env.parse_version("no version here") is None
    assert env.parse_version("") is None
    assert env.parse_version(None) is None


def test_requirements_are_all_exactly_pinned():
    # A range here would silently break byte-identical generation.
    for line in (env.REPO_ROOT / "requirements.txt").read_text().splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            assert "==" in line, f"unpinned requirement: {line}"


def test_generate_missing_project_is_a_validation_error(capsys):
    assert cli.main(["generate"]) == cli.VALIDATION_ERROR
    assert "generate:" in capsys.readouterr().err


def test_every_spec_command_is_registered():
    # The CLI names are a contract (5.1): CI and the CMake pre-build target
    # spell them. Registering them late means discovering a typo late.
    parser = cli.build_parser()
    action = next(a for a in parser._actions if a.dest == "command")
    assert set(action.choices) == set(cli.COMMANDS) | {"check-env"}


def test_current_scope_manifests_validate():
    root = Path(__file__).resolve().parents[2]
    assert validate_manifest(root / "interfaces/monochrome-frame.json", "interface") == []
    assert validate_manifest(root / "drivers/ssd1306/ssd1306.json", "driver") == []
    assert validate_manifest(root / "devkits/devkit-hw364a.json", "board") == []


def test_current_schema_catalog_and_manifests_validate():
    root = Path(__file__).resolve().parents[2]
    kinds = {path.stem: path for path in (root / "scripts/schemas").glob("*.json")}
    assert set(kinds) == {"project", "lock", "interface", "driver", "swc", "soc", "module", "board", "overlay"}
    for path in root.glob("interfaces/*.json"):
        assert validate_manifest(path, "interface") == []
    for path in root.glob("drivers/*/*.json"):
        assert validate_manifest(path, "driver") == []
    for path in root.glob("handcode/*/*.json"):
        assert validate_manifest(path, "swc") == []
    for path in root.glob("soc/*.json"):
        assert validate_manifest(path, "soc") == []
    for path in root.glob("modules/*.json"):
        assert validate_manifest(path, "module") == []
    for path in root.glob("devkits/*.json"):
        assert validate_manifest(path, "board") == []


def test_new_writes_a_current_scope_project(tmp_path, capsys):
    output = tmp_path / "project.json"
    assert cli.main(["new", "--reference", "hw364a-oled-demo", "--output", str(output)]) == cli.OK
    assert '"reference":"v01-hw364a-reference"' in output.read_text()
    assert "created:" in capsys.readouterr().out


def test_headless_edit_commands_preserve_valid_project(tmp_path):
    source = Path(__file__).resolve().parents[2] / "scripts/tests/fixtures/01-hw364a-oled-demo/project.json"
    project = tmp_path / "project.json"
    project.write_bytes(source.read_bytes())
    original_device = __import__("json").loads(project.read_text())["instances"]["devices"][0].copy()
    assert cli.main(["add", str(project), "module", "Spi"]) == cli.OK
    assert cli.main(["add", str(project), "device", "spareOled", "ssd1306", "bus=I2C0", "address=0x3D"]) == cli.OK
    assert cli.main(["configure", str(project), "--set", "project.version=0.2.0"]) == cli.OK
    assert cli.main(["remove", str(project), "device", "spareOled"]) == cli.OK
    data = __import__("json").loads(project.read_text())
    assert data["project"]["version"] == "0.2.0"
    assert "Spi" in data["modules"]
    assert all(item["instance"] != "spareOled" for item in data["instances"]["devices"])
    assert data["instances"]["devices"][0] == original_device
