"""Tests for current board reservation rules."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from scripts.wizard.core.allocate import find_conflicts, validate_hw364a_claims  # noqa: E402


def test_hspi_conflicts_with_the_hw364a_oled_pins():
    conflicts = validate_hw364a_claims([{"resource": "HSPI", "owner": "spi0"}])
    assert {(item["resource"], tuple(item["owners"])) for item in conflicts} == {
        ("GPIO12", ("onboardOled", "spi0")),
        ("GPIO14", ("onboardOled", "spi0")),
    }


def test_i2c_share_requires_an_explicit_compatible_key():
    claims = [
        {"resource": "I2C0", "owner": "onboardOled", "share_key": "i2c-master"},
        {"resource": "I2C0", "owner": "bme280", "share_key": "i2c-master"},
    ]
    assert find_conflicts(claims) == []
    claims[1].pop("share_key")
    assert find_conflicts(claims) == [
        {"resource": "I2C0", "owners": ("onboardOled", "bme280")}
    ]


def test_free_gpio_does_not_conflict():
    assert validate_hw364a_claims([{"resource": "GPIO13", "owner": "test"}]) == []
