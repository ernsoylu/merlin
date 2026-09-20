"""Small resource checks shared by the future allocation stage."""


HW364A_FIXED_CLAIMS = (
    {"resource": "GPIO12", "owner": "onboardOled"},
    {"resource": "GPIO14", "owner": "onboardOled"},
    {"resource": "I2C0", "owner": "onboardOled", "share_key": "i2c-master"},
)

HW364A_SPI_RESOURCES = {
    "HSPI": ("GPIO12", "GPIO13", "GPIO14", "GPIO15"),
}

# ADC2 on ESP32 is wired through the radio: selecting a radio reserves it, and
# a converter claim that survives that is a reading the radio can corrupt.
ESP32_RADIO_RESOURCES = {
    "WLAN": ("ADC2",),
    "BT": ("ADC2",),
}


def find_conflicts(claims):
    """Return incompatible duplicate resource claims as small dictionaries."""
    seen = {}
    conflicts = []
    for claim in claims:
        resource = claim["resource"]
        owner = claim["owner"]
        for prior in seen.get(resource, ()):
            compatible = (
                prior.get("share_key") is not None
                and prior.get("share_key") == claim.get("share_key")
            )
            if prior["owner"] != owner and not compatible:
                conflicts.append(
                    {
                        "resource": resource,
                        "owners": (prior["owner"], owner),
                    }
                )
        seen.setdefault(resource, []).append(claim)
    return conflicts


def _expand(claims, fixed, implied):
    """Add the physical resources each claim drags in with it."""
    expanded = list(fixed)
    for claim in claims:
        expanded.append(claim)
        for resource in implied.get(claim["resource"], ()):
            expanded.append({"resource": resource, "owner": claim["owner"]})
    return expanded


def validate_hw364a_claims(claims):
    """Check user claims against the soldered OLED and fixed HSPI pins."""
    return find_conflicts(
        _expand(claims, HW364A_FIXED_CLAIMS, HW364A_SPI_RESOURCES)
    )


def validate_esp32_claims(claims):
    """Check user claims against ESP32 resources a selected radio consumes."""
    return find_conflicts(_expand(claims, (), ESP32_RADIO_RESOURCES))
