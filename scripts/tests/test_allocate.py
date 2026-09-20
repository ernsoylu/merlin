"""Tests for current board reservation rules."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from scripts.wizard.core.allocate import (  # noqa: E402
    find_conflicts,
    validate_esp32_claims,
    validate_hw364a_claims,
)


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


def test_esp32_radio_reserves_adc2():
    conflicts = validate_esp32_claims(
        [
            {"resource": "WLAN", "owner": "wlan0"},
            {"resource": "ADC2", "owner": "batterySense"},
        ]
    )
    assert conflicts == [
        {"resource": "ADC2", "owners": ("wlan0", "batterySense")}
    ]


def test_esp32_adc1_is_unaffected_by_the_radio():
    assert (
        validate_esp32_claims(
            [
                {"resource": "WLAN", "owner": "wlan0"},
                {"resource": "ADC1", "owner": "batterySense"},
            ]
        )
        == []
    )


def test_esp32_radio_reserves_core_one():
    assert validate_esp32_claims(
        [
            {"resource": "WLAN", "owner": "wlan0"},
            {"resource": "CORE1", "owner": "controlTask"},
        ]
    ) == [
        {"resource": "CORE1", "owners": ("wlan0", "controlTask")}
    ]
