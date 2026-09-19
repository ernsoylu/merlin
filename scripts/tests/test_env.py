"""Tests for toolchain pin loading and version parsing.

These matter because check-env's whole job is being diagnosable: a banner it
misparses becomes a confident wrong answer about the toolchain.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from scripts.wizard import cli  # noqa: E402
from scripts.wizard.core import env  # noqa: E402


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


def test_unimplemented_commands_exit_with_env_error(capsys):
    assert cli.main(["generate"]) == cli.ENV_ERROR
    assert "not implemented" in capsys.readouterr().err


def test_every_spec_command_is_registered():
    # The CLI names are a contract (5.1): CI and the CMake pre-build target
    # spell them. Registering them late means discovering a typo late.
    parser = cli.build_parser()
    action = next(a for a in parser._actions if a.dest == "command")
    assert set(action.choices) == set(cli.COMMANDS) | {"check-env"}
