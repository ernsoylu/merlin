from pathlib import Path
import pytest

from scripts.tests.harness import assert_tree_equal, tree_diff
from scripts.wizard.core.generate import generate_project
from scripts.wizard.core import generate as generate_module
from scripts.wizard.core.lock import audit_lock


ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "scripts" / "tests" / "fixtures"


def _fixture_dirs():
    return sorted(path for path in FIXTURES.iterdir() if path.is_dir())


def test_reference_fixtures_are_byte_exact(tmp_path):
    for fixture in _fixture_dirs():
        project = fixture / "project.json"
        reference = fixture / "expected" / __import__("json").loads(project.read_text())["reference"]
        output = tmp_path / fixture.name
        lock = tmp_path / f"{fixture.name}.lock"
        generate_project(project, output, lock_path=lock)
        assert_tree_equal(output, reference)
        assert audit_lock(lock) == []


def test_generation_is_byte_deterministic(tmp_path):
    fixture = _fixture_dirs()[0]
    first = tmp_path / "first"
    second = tmp_path / "second"
    generate_project(fixture / "project.json", first, lock_path=tmp_path / "first.lock")
    generate_project(fixture / "project.json", second, lock_path=tmp_path / "second.lock")
    assert_tree_equal(first, second)


def test_frozen_generation_requires_and_audits_lock(tmp_path):
    fixture = _fixture_dirs()[0]
    lock = tmp_path / "project.lock"
    output = tmp_path / "code"
    with pytest.raises(ValueError, match="existing lock"):
        generate_project(fixture / "project.json", output, frozen=True, lock_path=lock)
    generate_project(fixture / "project.json", output, lock_path=lock)
    generate_project(fixture / "project.json", output, frozen=True, lock_path=lock)


def test_generation_requires_warning_acknowledgement(tmp_path):
    import json

    fixture = _fixture_dirs()[0]
    project = json.loads((fixture / "project.json").read_text())
    project.pop("acknowledgedWarnings", None)
    path = tmp_path / "project.json"
    path.write_text(json.dumps(project))
    with pytest.raises(ValueError, match="unacknowledged warnings"):
        generate_project(path, tmp_path / "code", lock_path=tmp_path / "project.lock")
    generate_project(path, tmp_path / "code", lock_path=tmp_path / "project.lock", acknowledgements=["VAL-003:fan", "VAL-003:statusLed"])


def test_changed_project_gets_generated_metadata(tmp_path):
    import json

    fixture = _fixture_dirs()[0]
    project = json.loads((fixture / "project.json").read_text())
    project["project"]["name"] = "climate-variant"
    path = tmp_path / "project.json"
    path.write_text(json.dumps(project))
    output = tmp_path / "code"
    generate_project(path, output, lock_path=tmp_path / "project.lock")
    assert "climate-variant" in (output / "main/merlin_project.json").read_text()
    assert "GENERATED FILE" in (output / "main/merlin_project.c").read_text()
    assert '"merlin_project.c"' in (output / "main/CMakeLists.txt").read_text()
    assert "merlin_lock_audit" in (output / "CMakeLists.txt").read_text()


def test_comparator_detects_one_byte_change(tmp_path):
    expected = tmp_path / "expected"
    actual = tmp_path / "actual"
    expected.mkdir()
    actual.mkdir()
    (expected / "sample.txt").write_text("abc\n")
    (actual / "sample.txt").write_text("axc\n")
    diff = tree_diff(actual, expected)
    assert "changed: sample.txt" in diff
    assert "-abc" in diff and "+axc" in diff


def test_failed_render_preserves_previous_output(tmp_path, monkeypatch):
    fixture = _fixture_dirs()[0]
    output = tmp_path / "code"
    output.mkdir()
    (output / "sentinel").write_text("keep\n")

    def fail(*args, **kwargs):
        raise RuntimeError("render interrupted")

    monkeypatch.setattr(generate_module.shutil, "copytree", fail)
    with pytest.raises(RuntimeError, match="render interrupted"):
        generate_project(fixture / "project.json", output, lock_path=tmp_path / "project.lock")
    assert (output / "sentinel").read_text() == "keep\n"
