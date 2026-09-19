#!/usr/bin/env python3
"""Assert the structured records emitted by the ESP32 QEMU smoke run."""

import json
import sys

records = [json.loads(line) for line in sys.stdin if line.lstrip().startswith("{")]
by_instance = {}
for record in records:
    if "instance" in record:
        by_instance.setdefault(record["instance"], []).append(record)

assert set(by_instance) >= {"ambientSensor", "enclosureSensor"}
for name in ("ambientSensor", "enclosureSensor"):
    assert by_instance[name], name
    assert all(item["health"] == "READY" for item in by_instance[name])
    sequences = [item["sequence"] for item in by_instance[name]]
    assert sequences[0] > 0 and sequences == sorted(set(sequences))
    assert all("temperatureCentiDegC" in item for item in by_instance[name])

print("qemu structured output ok")
