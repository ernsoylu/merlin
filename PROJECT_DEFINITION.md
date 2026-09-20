# Merlin ECU Project Wizard — Project Definition

**Document version:** 2.2.0 · **Status:** scope amended by owner request, 2026-09-19; current-scope schemas, fixtures and reference renderer implemented; final physical freeze deferred
**Supersedes:** 2.1.0 for ECU scope, target-specific contracts, and reference status. Working 2.2.0 schemas are checked in; final qualification remains conditional on the deferred physical evidence pass.
**Decisions on record:** native ESP-IDF CMake for ESP32; generate only hardware-proven reference paths. Owner expansion: add generic ESP8266 as an ECU target, HW-364A as a board profile, and SSD1306 as a reusable device driver automatically selected by that board. The hand-built ESP32/host and ESP8266/HW-364A bring-up reference slices are implemented; full ESP8266 qualification remains separate.

**Current hardware scope decision (2026-09-19):** available hardware is limited to development boards and the connected HW-364A OLED. Current work therefore prioritizes internal BSW/MCAL/OS/EcuM/Hm/Det/Log/Rte services, target-specific backends, hardware-peripheral and acceleration drivers, WLAN/BT capability paths, board profiles, and SSD1306/OLED behavior. Exact external-device drivers (BME280 first), external wiring/calibration/electrical qualification, and their related manifests/fixtures are deferred and non-selectable until the hardware is available. The existing BME280 code and fake transport are simulation/contract providers only.

---

## 1. Introduction

### 1.1 Purpose
A LEGO-style project generator for embedded ECUs, initially ESP32-WROOM/HW-394 and generic ESP8266 with optional HW-364A board defaults: the user composes AUTOSAR-Classic-*style* building blocks — MCAL modules, device-driver **instances**, application SWC **instances** — and the wizard generates a compilable, deterministic, supervision-capable project for the selected target SDK, repeatably and modifiably over the project lifetime.

### 1.2 Scope, Conformance & Disclaimer
This project **reuses AUTOSAR methodology and naming for structure and discipline. It is not an AUTOSAR Classic implementation and claims no conformance.** Per-module fidelity is declared explicitly in §2.2 (SWS-signature-compatible / name-inspired / no analog). Generated configuration is **link-time** configuration (compiled C); the term "post-build" is not used. The software is **not developed per ISO 26262 or IEC 61508 and carries no ASIL/SIL claim.**

**In scope:** wizard CLI; JSON data models + schemas; deterministic generation; resource/pin allocation; internal RTE, OS, EcuM, and supervision services; target-specific MCAL, hardware-peripheral/acceleration, WLAN/BT, and board-driver capability paths; simulated environmental data; ESP32 QEMU boot test model; Linux installation; two independently built ECU profiles and the connected HW-364A OLED bench.
**Out of scope (v1.x):** OTA, debugging UI, multi-ECU communication stacks.

Multiple supported ECU types means one target, image, configuration, and lock per project. Cross-ECU RTE connections, network coordination, and synchronized clocks are not part of this expansion. Generic ESP8266 and its HW-364A board profile are in the v0.1 reference/v1.0 generator scope, conditional on their qualification gates; the native bring-up reference is implemented and hardware-tested for boot/I2C/OLED transfer, but is not yet fully qualified. [ECU support and Luna handoff](docs/ecu-support.md) defines those gates and the driver compatibility matrix.

### 1.3 Positioning & Prior Art
- **Vs. Zephyr:** Zephyr already provides devicetree (≈ board manifests), bindings (≈ driver manifests), Kconfig (≈ module selection), *and* a task-watchdog facility. This project's differentiators are the **signal-level RTE**, **provider-agnostic port composition**, and **generated, integrated supervision** (task wrapper → Hm → EcuM as one contract). The devicetree **overlay** model is adopted for project-specific wiring (v1.1).
- **Vs. CubeMX:** strict RTE boundary, instance model, zero hand-code in the generated tree, deterministic regeneration.
- **Vs. EB tresos / Vector DaVinci:** manifest validation, dependency resolution, golden-fixture generator regression, per-ID validation severities.

### 1.4 References
R1 AUTOSAR Classic Layered Software Architecture · R2 AUTOSAR SWS: Mcu, Port, Dio, Adc, Pwm, Icu, Spi, Can, Wdg, Eth, Det, Os, EcuM, NvM, WdgM, Dem · R3 ESP-IDF v5.2.3 (FreeRTOS SMP, wdts, gpio, i2c, uart, ledc, intr_alloc, startup, build system, QEMU) · R4 FreeRTOS · R5 Bosch BME280 datasheet BST-BME280-DS002 · R6 espboards.dev/esp32/esp32-hw-394 · R7 JSON Schema Draft 2020-12.

R8 [HW-364A reference repository](https://github.com/peff74/esp8266_OLED_HW-364A/tree/15867f545c6fdb063a186e069f140c4b9e17ccc6), inspected 2026-09-19; R9 [Espressif ESP8266 RTOS SDK v3.4](https://github.com/espressif/ESP8266_RTOS_SDK/releases/tag/v3.4), its [support policy](https://github.com/espressif/ESP8266_RTOS_SDK/blob/master/SUPPORT_POLICY_EN.md), [Linux setup](https://github.com/espressif/ESP8266_RTOS_SDK/blob/master/docs/en/get-started/linux-setup.rst), and its [I2C API](https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/api-reference/peripherals/i2c.html), all inspected 2026-09-19. R8 is board/example evidence, not proof of Merlin compatibility. R9 v3.4/GCC 8.4.0 is the candidate native backend pin; exact commit/tool archive, build and runtime qualification remain required.

### 1.5 Terms
ASW · SWC · RTE · BSW · MCAL · EcuM · Hm (Health Monitor) · Det · IoHwAb · Runnable · PortInterface (typed, provider-agnostic) · **type vs. instance** (driver/SWC types are repo components; instances are project-level entities) · sample contract · lockfile.
⚠ *Port* = pin-configuration MCAL module; RTE *port* = component interface endpoint. The document always qualifies which.

---

## 2. System Architecture

### 2.1 Layers & Access Rules

| Rule | Statement | Primary enforcement |
|---|---|---|
| **R1** | SWCs access system functions **only** via generated `Rte_*` APIs; ASW never includes driver or ESP-IDF headers | Include/symbol lint + **negative compile tests** (a file that includes `bme280.h` from SWC context must fail CI) |
| **R2** | Device drivers access hardware **only** via MCAL component headers | Same mechanism |

Component `REQUIRES` matrices (§7.3) are **defense-in-depth only** — ESP-IDF auto-exposes `freertos`/HAL/log to all components, so `REQUIRES` alone cannot prove the boundary. BSW-internal paths (EcuM→MCAL init, Hm→Det) are permitted below the RTE and documented in §6.9. A component without a manifest is **not** auto-classified as a CDD; integration metadata is mandatory or generation refuses the component.

### 2.2 Module Catalog & AUTOSAR Fidelity

**v1.0 MCAL catalog:** Mcu, Port, Dio, I2c, Spi, Uart, Adc, Pwm, Gpt, Wdg. Each module becomes selectable only for qualified ECU backends; catalog membership alone is not implementation evidence.
**v1.0 BSW (auto-managed):** Os, EcuM, Det, Hm, Log.
**Planned (visible, non-selectable, version-tagged):** Twai (v1.1), Icu/PCNT subset (v1.1), Eth (v1.2), Sdio (v1.2), Touch, Rmt, Mcpwm, Dac, Aes/Sha/Rsa/Rng (v1.2).
**Never user-selectable:** Dma, Intm, Flash, ULP (implicit/advanced). **FlashEnc/eFuse:** one-way hardware operations — behind `device-security --expert` (activated v1.2), never in a peripheral checklist.

| Module | AUTOSAR analog | Fidelity |
|---|---|---|
| Mcu, Port, Dio, Adc, Pwm, Spi, Wdg | SWS exists | name + partial signature compatibility; **not conformant** |
| Icu | Icu SWS | planned v1.1, **partial**: pulse counting only |
| Eth | Eth SWS | planned v1.2, name-inspired |
| Twai | Can SWS | planned v1.1, name-inspired |
| Os | Os SWS | subset: tasks/counters; no schedule tables; FreeRTOS mutexes give priority **inheritance**, not ceilings (documented limitation) |
| EcuM, NvM (v1.1) | SWS exists | fixed subset (see §6.6) |
| Hm | WdgM/Dem | inspired: supervision + fault-qualification subset |
| Uart, I2c, Dac, Touch, Rmt, Mcpwm, Sdio | none in Classic | name-inspired |

**SWC/driver instance calling convention (uniform):** runnables take an instance context — `void ClimateController_Run(ClimateController_CtxType* self)`; drivers take instance handles — `void Bme280_MainFunction_High(Bme280_InstanceType inst)`. Single- and multi-instance types use the same convention.

### 2.2.1 ECU support matrix

| ECU / board profile | Reference workload | Backend | Current evidence |
|---|---|---|---|
| `esp32` / HW-394 | Simulated environmental provider, climate controller, PWM fan | ESP-IDF 5.2.3, native CMake | Internal runtime, recovery/failsafe path and host tests implemented; external BME280 deferred |
| `esp8266` / generic | Explicitly chosen drivers and wiring; no default OLED | ESP8266 RTOS SDK v3.4, GCC 8.4.0 | Baseline backend and I2C adapter implemented; qualification pending |
| `esp8266` / HW-364A | Default SSD1306 instance; demo binds display SWC and health reporting | Same generic ESP8266 backend | Native boot/OLED transfer baseline passed; internal backend and peripheral measurements pending |
| Future ESP32-S3/C3 | To be defined in v1.2 | Target-specific SDK profiles | Planned |

The module catalog above is a capability catalog, not a promise that every ECU implements every module. The initial ESP8266 subset requires Port/Dio, I2c, Uart, time/watchdog services and Os/Rte/EcuM/Hm/Det/Log, with WLAN and available hardware-peripheral paths added only when the SDK and board prove them. The current device path is SSD1306 through MCAL I2c; BME280 remains cataloged as a simulation provider and deferred external driver, not a selectable physical target. HW-364A adds an SSD1306 instance by default. PWM, ESP32 LEDC/RMT/ADC2, Bluetooth, and multi-core APIs are not inherited by ESP8266. Unsupported or deferred selections fail validation (VAL-026). HW-364B is mentioned upstream but is not a separately qualified Merlin target.

### 2.3 Composition Model

| Concept | Example |
|---|---|
| Driver type | `bme280` (repo `drivers/bme280/`) |
| Device instance | `ambientSensor` (bme280 @ I2C0:0x76), `enclosureSensor` (bme280 @ I2C0:0x77) |
| SWC type | `ClimateController` (repo `handcode/climatecontroller/`) |
| SWC instance | `cabinController` |
| Interface | `EnvironmentalData` (typed, versioned, provider-agnostic) |
| Connection | `ambientSensor.Env → cabinController.Ambient` (always port-to-port) |

Actuator/IO access is unified into the same model: the wizard generates an **IoHwAb** component whose instances implement hardware interfaces (`PwmDutyCycle`, `DioLevel`). There is exactly **one** wiring mechanism — ports.
**Fan-in/fan-out:** one provider port may feed multiple consumers (each gets its own implicit snapshot); one requirer port may be fed by exactly one provider (multiple providers → generation error).

The HW-364A composition is `DisplayDemo.DisplayOut → onboardOled.Frame`, with `onboardOled.Health → Hm`. The display SWC uses a provider-agnostic `MonochromeFrame` port; only `Drv_Ssd1306` knows OLED commands and only MCAL knows bus pins/SDK calls. Rendering and packing are host-testable; initialization and bounded frame transfer are target-tested. The exact frame and health contracts are specified in [the ECU support document](docs/ecu-support.md#oled-runtime-contract).

Board selection adds hardware instances and reserves their physical resources; application/demo selection supplies SWCs and bindings. HW-364A automatically adds exactly one `ssd1306` instance and its I2C dependency/reservations. Generic ESP8266 adds no devices automatically. The same SSD1306 driver can serve an explicitly wired external panel on either ECU. Disabling a board device's software does not free pins/address still occupied by soldered hardware.

### 2.4 EcuM
Generated state machine: **STARTUP → RUN ⇄ DEGRADED → SHUTDOWN / SAFE_HALT**. EcuM owns initialization sequencing, startup error handling, degraded-mode policy, controlled reset, and shutdown. Each device manifest declares `initPolicy.onFailure ∈ {degradable, inhibit, abort}`; EcuM applies it to dependents (components whose required ports bind to the failed device). EcuM executes as a scheduled runnable (§6.9) — `app_main` returns and the main task is deleted, so EcuM must have a scheduler slot like everything else.

---

## 3. Repository, Source of Truth & Reproducibility

### 3.1 Layout (original project folders preserved in bold mapping)

```
merlin/
├── code/                    # generated ESP-IDF project — generated artifacts ONLY
├── test/                    # generated host-test project + QEMU model
├── scripts/
│   ├── wizard/              # cli.py, steps/, core/{model,validate,allocate,rte,generate}
│   ├── templates/           # Jinja2 (version-pinned)
│   └── schemas/             # project, lock, interface, driver, swc, soc, module, board, overlay
├── interfaces/              # PortInterface catalog (versioned)
├── drivers/<type>/          # <type>.c, <type>_calc.c, <type>.h, <type>.json, tests/
├── handcode/<swctype>/      # <swctype>.c/.h, <swctype>.json, tests/
├── soc/esp32.json           # SoC capability profile
├── modules/esp32-wroom-32.json   # module wiring facts
├── devkits/devkit-*.json    # board header facts, pull-ups, onboard devices
├── overlays/                # per-project wiring deltas (v1.1)
├── v01-reference/           # hand-built ESP32/host current-scope reference; future golden fixture #0 (§11)
├── v01-hw364a-reference/    # hand-built ESP8266 + OLED reference, fixture #1 candidate
├── project.json             # composition entry point
├── project.lock             # generation record (§3.3)
├── install.sh · requirements.txt · docs/
```
Original mapping: `code/`→code, `scripts/`→scripts, `test/`→test, `handcode/`→handcode, `drivers/`→drivers, `devkit-hw394.json`→`devkits/devkit-hw394.json`, `project.json`→project.json, `install.sh`→install.sh.

### 3.2 Precedence & Regeneration Boundary
`project.json` **requests**; the lock **records and pins**. Precedence for any configurable value: explicit project.json value > instance config > manifest default > interface default — conflicts reported, never silent. Expand board-provided instances/defaults before resolution; re-expansion is idempotent. Fixed physical wiring is a constraint, not an overridable default: contradictory settings are rejected unless the configuration records a verified variant/physical change. All JSON is RFC 8259 (no comments). **Regeneration boundary:** edits to manifests, interfaces, or schemas require regeneration; function-body edits do not. `wizard.py audit` (wired as a **CMake pre-build target**) verifies lock source hashes against the working tree, so the lock cannot drift silently.

### 3.3 Lockfile & Determinism
- `project.lock` records: generator + template versions, per-source content hashes (drivers/SWCs/interfaces), soc/module/board/overlay versions, schema versions, ECU profile and pinned SDK family/revision + toolchain + effective build configuration, resolved dependencies, accepted warnings (ID + object + config-hash of the affected subtree).
- **Frozen generation** (`generate --frozen`) rejects any drift; `wizard.py resolve` refreshes the lock explicitly. The lock never overrides intent.
- Generation renders to a staging directory; output replacement is atomic after successful validation + render. Interrupted generation leaves the previous output intact and buildable.
- Determinism: all iteration sorted at boundaries; Jinja2 pinned; no timestamps by default; CI double-generate byte-diff.
- **PlatformIO:** not a v1.0 target (cannot honor the pinned IDF). Returns v1.1+ as explicitly unpinned, best-effort, outside the reproducibility guarantee.
- `install.sh`: idempotent, `--dry-run`, non-root (sudo only for system packages), ends with `wizard.py check-env`. Current pins cover ESP-IDF 5.2.3, Espressif QEMU, Python ≥3.10 + jinja2/jsonschema/questionary/pytest. Planned target selection must isolate SDK environments, compiler prefixes, build directories and dependency pins. The present installer/checker does not support ESP8266; its QEMU check is ESP32-only.

---

## 4. Data Models (2.2.0 working revision; final freeze follows evidence)

The JSON examples in §4.1–4.5 describe the original ESP32 climate path. The
working 2.2.0 schemas and current-scope manifests now live under
`scripts/schemas/`, `interfaces/`, `drivers/`, `handcode/`, `soc/`, `modules/`
and `devkits/`. They are not fully frozen until the deferred physical evidence
pass is complete; unverified electrical values remain explicit `null`/status
fields and cannot make a capability selectable. Driver/interface semantic
versions remain independent.

### 4.1 Interface catalog — `interfaces/environmental.json`

```json
{
  "schemaVersion": "2.1.0",
  "interface": {
    "name": "EnvironmentalData",
    "version": "1.1.0",
    "kind": "sample-group",
    "elements": [
      { "name": "Temperature", "type": "float32", "unit": "degC", "range": [-40.0, 85.0] },
      { "name": "Humidity",    "type": "float32", "unit": "%RH",  "range": [0.0, 100.0] },
      { "name": "Pressure",    "type": "float32", "unit": "Pa",   "range": [30000.0, 110000.0] }
    ],
    "metadata": ["qualityPerElement", "sampleTimeUs", "sequence"]
  }
}
```

`kind` ∈ `sample-group` (coherent measurement set + metadata) · `event` (queued, v1.1) · `state` (latest-value scalar; schema 2.2.0 also permits bounded structured state for `MonochromeFrame`) · `parameter` (calibratable, v1.1 Cal). Planned standard interfaces: `HealthReport`, `PwmDutyCycle`, `DioLevel`, `PidParams`. **Interface semver rules:** adding an element = minor; unit change or range narrowing = major. `SUBSTITUTED` quality (v1.1) requires a per-element declared substitute value in the interface; interfaces without one never produce it. The provider owns the substitution decision and applies only declared values; consumers preserve `SUBSTITUTED` as distinct from `VALID`. Metadata types (normative, in `Rte_Type.h`): `sampleTimeUs` = `int64` µs from `esp_timer_get_time()` (light-sleep behavior per pinned IDF version, recorded in lock); `sequence` = `uint32`, wrap-safe comparison; `quality` **per element**.

### 4.2 Driver type manifest — `drivers/bme280/bme280.json` (design example; budgets unmeasured)

```json
{
  "schemaVersion": "2.1.0",
  "driverType": {
    "name": "bme280", "version": "2.1.0",
    "description": "Bosch BME280 environmental sensor (I2C)",
    "multipleInstances": true
  },
  "requiresModules": ["I2c"],
  "bus": {
    "requires": "i2c",
    "addressSelection": { "type": "strap", "pin": "SDO", "options": ["0x76", "0x77"] },
    "maxSpeedHz": 100000
  },
  "provides": [
    { "port": "Env", "interface": "EnvironmentalData", "interfaceVersion": "^1.1.0" }
  ],
  "runnables": [
    { "name": "MainFunction_High", "symbol": "Bme280_MainFunction_High",
      "trigger": "periodic", "cycleMs": 10 }
  ],
  "acquisition": {
    "pattern": "start-check-read",
    "statesPerSample": 3,
    "acquisitionPeriodMs": 30
  },
  "executionContract": {
    "cpuUs": 300,
    "maxElapsedMs": 3,
    "transactionTimeoutMs": 2,
    "lockTimeoutMs": 0,
    "retries": 1,
    "retryPlacement": "next-activation",
    "recovery": { "afterConsecutiveFails": 3, "cooldownActivations": 10,
                  "budgetUs": 400, "withinElapsedBudget": true }
  },
  "health": {
    "lifecycle": ["UNINIT", "INIT", "READY", "DEGRADED", "FAILED"],
    "commFaults": ["NACK", "TIMEOUT", "BUS_RECOVERED"],
    "deviceFaults": ["CHIP_ID_MISMATCH", "MEAS_STUCK", "COMP_RANGE"]
  },
  "initPolicy": { "onFailure": "degradable" },
  "resources": { "ramBytesPerInstance": 96, "stackBytes": 3072, "queueBytes": 0, "iramBytes": 0 }
}
```

Key semantics: `acquisitionPeriodMs` is what consumer `maxAgeMs` is validated against — **activation rate ≠ sample rate**. `retries: 1` = one additional attempt **on a later activation**, never inside the current one — this is what makes `maxElapsedMs` close (VAL-023). `addressSelection.type: strap` means the address is set by wiring (SDO pin): on collision the wizard **proposes a wiring change** and requires the configuration to describe the physical board; it never silently rewrites an address.

### 4.3 SWC type manifest — `handcode/climatecontroller/climatecontroller.json` (design example)

```json
{
  "schemaVersion": "2.1.0",
  "swcType": {
    "name": "ClimateController", "version": "1.0.0",
    "multipleInstances": true, "callingConvention": "context-pointer"
  },
  "ports": {
    "requires": [
      { "port": "Ambient",   "interface": "EnvironmentalData", "interfaceVersion": "^1.1.0",
        "access": "implicit", "maxAgeMs": 150 },
      { "port": "Enclosure", "interface": "EnvironmentalData", "interfaceVersion": "^1.1.0",
        "access": "implicit", "maxAgeMs": 150 }
    ],
    "provides": [
      { "port": "FanOut", "interface": "PwmDutyCycle", "interfaceVersion": "^1.0.0",
        "failsafe": { "dutyPercent": 100 } }
    ],
    "parameters": [
      { "port": "FanPid", "interface": "PidParams", "interfaceVersion": "^1.0.0",
        "values": { "Kp": 2.0, "Ki": 0.05, "Kd": 0.0, "outMin": 0.0, "outMax": 100.0 } }
    ]
  },
  "runnables": [
    { "name": "Init", "symbol": "ClimateController_Init", "trigger": "init" },
    { "name": "Run_100ms", "symbol": "ClimateController_Run_100ms",
      "trigger": "periodic", "cycleMs": 100, "wcetUs": 400 }
  ],
  "resources": { "ramBytes": 128, "stackBytes": 3072 }
}
```

The SWC names **ports and interfaces, never providers** — provider binding happens at composition (project.json `connections`). Parameter ports take a const provider in v1.0/v0.1; the v1.1 Cal component replaces the provider without touching SWC signatures.

### 4.4 `project.json` — final, for the reference path (climate-demo)

```json
{
  "schemaVersion": "2.1.0",
  "generatorVersion": "0.1.0",
  "project": { "name": "climate-demo", "version": "0.1.0",
               "author": "Jane Doe", "company": "ACME Robotics" },
  "target": {
    "soc": "soc/esp32.json",
    "module": "modules/esp32-wroom-32.json",
    "board": "devkits/devkit-hw394.json",
    "overlays": [],
    "espIdf": "5.2.3",
    "radio": { "wifi": false, "bt": false }
  },
  "modules": ["Mcu","Port","Dio","I2c","Pwm","Wdg","Os","EcuM","Det","Hm","Log"],
  "buses": {
    "I2C0": {
      "type": "i2c", "speedHz": 100000,
      "pins": { "sda": "IO21", "scl": "IO22" },
      "pullups": { "present": true, "valueOhms": 4700, "source": "board" }
    }
  },
  "instances": {
    "devices": [
      { "instance": "ambientSensor",   "type": "bme280", "bus": "I2C0", "address": "0x76" },
      { "instance": "enclosureSensor", "type": "bme280", "bus": "I2C0", "address": "0x77" }
    ],
    "swcs": [
      { "instance": "cabinController", "type": "ClimateController" }
    ],
    "iohwab": [
      { "instance": "fan", "type": "PwmChannel", "interface": "PwmDutyCycle",
        "pin": "IO2", "speedMode": "high", "timer": 0, "channel": 0, "frequencyHz": 25000,
        "input": { "maxAgeMs": 300, "holdCycles": 2, "failsafe": { "dutyPercent": 100 } },
        "idleLevel": "low" }
    ]
  },
  "tasks": [
    { "name": "T10",  "periodMs": 10,  "phaseMs": 0, "deadlineMs": 2,  "priority": 18,
      "core": 0, "stackBytes": 3072, "overrunPolicy": "skip", "release": "tick" },
    { "name": "T100", "periodMs": 100, "phaseMs": 0, "deadlineMs": 10, "priority": 14,
      "core": 0, "stackBytes": 3072, "overrunPolicy": "skip", "release": "tick" },
    { "name": "T500", "periodMs": 500, "phaseMs": 0, "deadlineMs": 20, "priority": 10,
      "core": 0, "stackBytes": 4096, "overrunPolicy": "skip", "release": "tick" }
  ],
  "runnableMap": {
    "T10":  ["ambientSensor.MainFunction_High", "enclosureSensor.MainFunction_High"],
    "T100": ["cabinController.Run_100ms", "fan.Run_100ms"],
    "T500": ["ecum.MainFunction_500ms", "hm.MainFunction_500ms", "log.Drain"]
  },
  "connections": [
    { "from": "ambientSensor.Env",      "to": "cabinController.Ambient" },
    { "from": "enclosureSensor.Env",    "to": "cabinController.Enclosure" },
    { "from": "cabinController.FanOut", "to": "fan.Duty" }
  ],
  "supervision": {
    "wakeJitterToleranceTicks": 1,
    "deadlineChecks": true,
    "twdt": { "timeoutS": 5, "feed": "per-activation", "subscribeAfterGate": true },
    "debounce": { "failedCyclesToSet": 3, "passedCyclesToHeal": 5 },
    "escalation": [
      { "on": "RTF-005_INIT_FAILED", "action": "degrade", "scope": "dependents" },
      { "on": "RTF-006_COMM",        "action": "degrade", "scope": "device" },
      { "on": "RTF-002_DEADLINE",    "action": "count", "limit": 5, "then": "controlledReset" },
      { "on": "RTF-004_STALE",       "action": "notifyConsumer" }
    ],
    "resetForensics": {
      "rtcCounter": true, "logResetReason": true,
      "bootLoop": { "maxResets": 5, "windowS": 300, "terminalState": "SAFE_HALT" }
    }
  },
  "acknowledgedWarnings": [
    { "id": "VAL-003", "object": "fan.pin",
      "configHash": "sha256:9f2c1e6a4b7d8305c1e2f4a6b8d0c3e5f7a9b1d3e5f7092a4b6c8d0e2f4a6b8c" }
  ]
}
```

`runnableMap` arrays are **ordered — order within the activation is normative** (here: the two same-rate sensors are serialized by declared runnable order; the bus is the serializer; no offset fields are involved). `target.radio` drives sdkconfig, core-1 reservation, ADC2 rules, and entropy rules, and is required even when false.

### 4.5 Hardware Description Tiers

Effective capability of any pin/resource = **SoC ∧ module ∧ board ∧ overlay**.

`soc/esp32.json` (abridged, corrected facts):

```json
{
  "schemaVersion": "2.1.0",
  "soc": { "name": "ESP32", "cores": 2 },
  "strappingPins": ["IO0", "IO2", "IO5", "IO12", "IO15"],
  "inputOnlyPins": { "pins": ["IO34","IO35","IO36","IO37","IO38","IO39"],
                     "internalPullups": false },
  "nativeMuxPolicy": {
    "thresholdMHz": 40,
    "note": "PROJECT POLICY (conservative), not a silicon limit. GPIO matrix supports higher rates with degraded timing; threshold configurable per SoC profile."
  },
  "peripherals": {
    "i2c":   { "instances": 2, "nativePins": "none (matrix-routed only)" },
    "spi":   { "instances": ["SPI0", "SPI1", "SPI2", "SPI3"],
               "generalPurpose": ["SPI2", "SPI3"],
               "nativePins": { "SPI3": { "clk": "IO18", "mosi": "IO23", "miso": "IO19", "cs": ["IO5"] } } },
    "uart":  { "instances": 3, "matrixRouted": true },
    "adc":   { "units": 2, "adc2SharedWithRadio": true },
    "ledc":  { "groups": [ { "name": "high", "channels": 4, "timers": 4 },
                           { "name": "low",  "channels": 4, "timers": 4 } ] },
    "pcnt":  { "units": 8 }, "rmt": { "channels": 8 }
  }
}
```

`modules/esp32-wroom-32.json` (abridged) — the module tier catches real facts:

```json
{
  "schemaVersion": "2.1.0",
  "module": { "name": "ESP32-WROOM-32" },
  "blockedPins": { "IO6": "SPI flash", "IO7": "SPI flash", "IO8": "SPI flash",
                   "IO9": "SPI flash", "IO10": "SPI flash", "IO11": "SPI flash",
                   "IO37": "not bonded out", "IO38": "not bonded out" }
}
```

`devkits/devkit-hw394.json` (abridged): header availability per pin; onboard devices (LED on IO2, declared `idleLevel`); bus pull-ups (`{present, valueOhms, source}` per bus); default console reservation UART0@IO1/IO3 (**a default reservation, overridable** — UART is matrix-routable); per-IO declared electrical idle level for the reset window (§6.5).

---

### 4.6 Target and HW-364A profile contract (working schema 2.2.0; physical qualification deferred)

`target` gains a required `ecu` identifier (`esp32` or `esp8266`) and an SDK object containing family and exact revision; the existing `target.espIdf` value maps to the ESP32 SDK object. Keep the SoC/module/board/overlay tiers, radio flags, and target flash configuration. The ECU/SDK selection is independent of generic versus HW-364A board selection. Do not accept incompatible SDK/SoC pairs, unqualified profiles, unavailable peripherals, or a core outside the selected SoC (VAL-026). Lock and audit include this selection and resolved board devices/reservations so frozen generation cannot silently switch targets. A missing SDK pin prevents the ESP8266 profiles from becoming selectable.

Current artifacts are `soc/esp8266.json`, module manifests named after the identified modules/packages, generic ESP8266 wiring profiles, `devkits/devkit-hw364a.json`, reusable `drivers/ssd1306/`, `interfaces/monochrome-frame.json`, and a display-demo SWC manifest. Do not infer the module identity from the board sales name. Board fields distinguish reference-derived values, physical confirmation, and measurements; unresolved values block qualification rather than receiving invented defaults.

HW-364A reserves its onboard display's bus pins and 7-bit address from the verified wiring record in [ECU support](docs/ecu-support.md#board-evidence). GPIO numbers are authoritative; D-labels are annotations. Treat the onboard OLED as an ordinary driver instance in resource allocation, binding, health and initialization policy. Its `initPolicy.onFailure` is `degradable`: report the display fault and continue serial diagnostics/supervision. A disconnected OLED cannot provide a visible error indication.

The initial `hw364a-oled-demo` project has Wi-Fi disabled, no Bluetooth, one logical core, one bounded display-transfer runnable and health/EcuM/log scheduling. Task periods, priorities, chunk size, timeouts, memory and recovery budgets come from that target's measurements; the climate project's values are not copied. A native ESP8266 backend must pass §6.10 before this project is a qualified reference.

HW-394 example values also require board inspection. The example's LED/fan IO2 use, pull-ups, and reset levels are not established by the serial boot smoke test.

---

## 5. Wizard Specification

### 5.1 CLI

| Command | Behavior |
|---|---|
| `new` | Interactive S1→S9 over the two qualified reference compositions; `--non-interactive` is deterministic |
| `configure` | Load + edit existing project.json |
| `add / remove <device-instance \| module \| swc-instance> NAME` | Partial re-run: dependency resolve → allocate → RTE → regenerate |
| `generate [--non-interactive] [--frozen] [--ack VAL-ID:object …]` | Generation; `--frozen` rejects lock drift; warnings acknowledged **per ID + object**, bound to the config-hash of the affected subtree |
| `validate project.json` | Schema + semantic validation only |
| `resolve` | Explicitly refresh the lock after source/toolchain changes |
| `allocate` | Resource/pin allocation only (sticky — see §5.4) |
| `rte` | Port wiring only |
| `audit` | Verify lock source hashes vs working tree (also a CMake pre-build target) |
| `check-env` | Toolchain verification (used by install.sh); planned target selector verifies only the chosen SDK/backend |
| `device-security --expert` | Guarded one-way eFuse/flash-encryption flow — **activated v1.2** |
| `print-schema [project\|lock\|interface\|driver\|swc\|soc\|module\|board]` | Dump schemas (LLM/docs context) |

Exit codes: 0 OK · 1 validation error · 2 environment error. PlatformIO output target: v1.1+, unpinned.

### 5.2 Interactive Steps
S1 metadata → S2 target (ECU + soc/module/board/overlay + pinned SDK + radio flags; expand board devices/reservations) → S3 modules (the qualified subset of the selected reference is shown) → S4 driver types & instances → S5 buses & addresses → S6 resource & pin allocation (§5.4) → S7 tasks & runnable mapping → S8 port wiring → S9 review (VAL report, diff vs previous project.json, lock preview) → generate (staged, atomic). In the current-scope CLI, S3–S8 are derived from the selected qualified reference composition and displayed in the review; arbitrary unqualified composition authoring remains outside the v1.0 generator boundary.

### 5.3 Dependency Resolution
Selecting a device instance auto-selects `requiresModules` (reported: `[+] I2c auto-selected (required by bme280)`) and reserves its bus instance, peripheral controller, and timer/channel resources. Board defaults first expand into normal device instances: selecting HW-364A adds `onboardOled`/`ssd1306` and its verified bus resources, reported in the same review. Generic ESP8266 starts with no onboard device defaults. Removal of a module in use is refused (VAL-006). Shared I2C devices use one compatible bus definition with distinct addresses; default expansion cannot create duplicate instances or silently reassign resources.

### 5.4 Resource & Pin Allocation

**Allocation domain** — a target-specific *resource table*, not just pins. ESP32 resource kinds include: pins; I2C/SPI/UART controller instances; LEDC timers & channels **per speed group**; ADC units & channels; RMT channels.

**Exclusivity vs. compatible sharing:** a resource is exclusive by default; PWM channels may **share a timer when their configuration matches** (frequency/resolution) — the allocator permits compatible sharing and rejects incompatible sharing (VAL-001 generalizes to all resource kinds).

**Native-mux rule (project policy, labeled as such):** functions above the SoC-profile threshold (default 40 MHz) bind only to pins whose `native_mux` lists that function (VAL-016). I2C has **no** native-mux constraint on ESP32 (matrix-only peripheral); its constraint is electrical — see VAL-018.

**Hard blocks:** apply the selected SoC/module/board constraints. ESP32 examples: module-blocked pins (GPIO6–11 flash; GPIO37/38 absent on WROOM-32); output or pull configuration on input-only pins (VAL-002); ADC2 channels while radio is active (VAL-004, Error — evaluated when radio modules become selectable, v1.1+).

**Electrical check (computed, not labeled):** I2C rise time ≈ 0.847·R·C must satisfy the mode's rise-time limit (1000 ns @100 kHz, 300 ns @400 kHz per I2C-bus spec) using the declared pull-up value and an assumed/declared bus capacitance. Above 100 kHz, external pull-ups are **required** (VAL-018 Error if unmet). No unconditional claims about internal pull-ups are made.

**Sticky-assignment invariant (normative):** existing allocations are constraints, not suggestions. `allocate` and `add` never move an existing assignment; changing one requires explicit release of that exact resource (VAL-015). A soldered board must survive regeneration.

**Pin-configuration ownership:** each pin is configured exactly once at startup, by its owning peripheral instance's generated configuration or by `Port` for plain GPIO. Allocator exclusivity makes double-configuration structurally impossible.

### 5.5 Validation Framework

**Generation-time (VAL):**

| ID | Rule | Severity |
|---|---|---|
| VAL-001 | Duplicate/incompatible allocation of any resource (pin, controller, timer, channel, ADC unit) | Error |
| VAL-002 | Output or pull config on input-only pin | Error |
| VAL-003 | Strapping pin assignment (IO0/2/5/12/15) | Warning (per-ID ack) |
| VAL-004 | ADC2 channel while radio active | Error (v1.1+) |
| VAL-005 | Bus address collision; strap devices require wiring proposal; software-addressed auto-resolve | Error |
| VAL-006 | Unmet or contested module dependency | Error |
| VAL-007 | Required port without bound provider (port endpoints) | Error |
| VAL-008 | Any period/phase/deadline not an integer multiple of the RTOS tick (1 ms @ 1000 Hz) | Error |
| VAL-009 | Runnable cycle not an integer multiple of its task period | Error |
| VAL-010 | Console (UART0) reservation changed or removed | Info |
| VAL-011 | Consumer `maxAgeMs` infeasible vs provider `acquisitionPeriodMs` (+ jitter/debounce) | Warning |
| VAL-012 | **RTA**: response-time analysis `R_i = C_i + B_i + Σ_{j∈hp(i)} ⌈R_i/T_j⌉·C_j` to fixed point vs `D_i`, using declared `wcetUs`; blocking terms only where they can occur (v1.0: B=0 under co-location). **A planning check, not a schedulability proof.** | Info |
| VAL-013 | Schema violation (any manifest) | Error |
| VAL-014 | Version mismatch (project pin vs manifest version) | Error |
| VAL-015 | Reallocation of an existing assignment without explicit release | Error |
| VAL-016 | Above-threshold function bound to non-native-mux pin | Error |
| VAL-017 | Bus speed clamped to slowest attached device | Info |
| VAL-018 | Computed I2C rise-time violation; >100 kHz without external pull-ups | Error |
| VAL-019 | Devices on one bus instance mapped to different tasks (v1.0 co-location rule; lifted by v1.1 async) | Error |
| VAL-020 | Interface incompatibility at a connection (per VAL-025 rules) | Error |
| VAL-021 | Declared DRAM/IRAM budget vs SoC memory map (incl. radio stack reservation) | Warning |
| VAL-022 | Crypto selected with Rng and no radio (weak-entropy path) | Error (v1.2+) |
| VAL-023 | Execution-contract closure: **generator derives** `maxElapsedMs ≥ transactionTimeoutMs + recovery.budgetUs` (when in-budget); rejects declarations that don't close | Error |
| VAL-024 | Interface version incompatibility between connection endpoints (semver rules §4.1) | Error |
| VAL-025 | Structural binding: provider element range ⊆ consumer accepted range; exact unit match; no implicit type conversion | Error |
| VAL-026 | ECU/SoC/SDK compatibility and qualified capability selection: exact SDK pin, valid core/peripherals, board reservations, supported supervision and build backend | Error |

**Runtime faults (RTF), raised by generated code, qualified by Hm:** RTF-001 data-range violation (per element) · RTF-002 deadline overrun · RTF-003 missed/late activation · RTF-004 stale input · RTF-005 init failure · RTF-006 bus communication fault · RTF-007 NvM block CRC invalid (v1.1).

Errors block generation; warnings require per-ID acknowledgment (interactive) recorded in the lock (headless).

---

## 6. Runtime Specification

§6.1–6.9 describe the ESP32 reference contract. Portable semantics (sample age, RTE boundaries, bounded work, fault qualification and safe shutdown) apply to both ECUs; ESP32-specific API names, core placement and watchdog mechanisms require the explicit mapping in §6.10 on ESP8266. A missing equivalent blocks qualification rather than silently weakening the contract.

### 6.1 Tasks & Timing Model

- One FreeRTOS task per configured task; the **generated wrapper** performs: release wait → record actual wake tick → run runnables **in declared order** → deadline check → TWDT feed.
- **Tick:** generator emits `CONFIG_FREERTOS_HZ=1000` and **validates the effective sdkconfig** (defaults do not override an existing full config). Justification: deadline/phase resolution and T10 release granularity. Documented tradeoff: tick-based release carries ±1 ms jitter (10% of T10), correlated with system load; per-task `release: "gptimer"` (GPTimer-notification release) is the v1.1 option for loops needing better.
- **Priority policy:** application tasks use priorities 1–19. 20+ reserved for IDF (esp_timer=22, WiFi=23, IPC=24). The interference model is **derived from enabled services**: with radio on, lwIP runs at 18 — the generator caps application priorities below the highest enabled framework task (default cap 17) and reports the derived budget.
- **Core affinity:** control tasks default core 0; producer/consumer pairs pinned to the **same core** by the wizard (avoids cross-core locking); core 1 reserved for radio/protocol stacks when enabled. Same-core placement does **not** remove the need for synchronization (preemption remains) — it removes cross-core lock jitter only.
- **Overrun policies (mathematical definitions):**
  - *skip*: discard all releases up to the next strictly-future boundary, increment skip counter, do not execute. Implementation note: `vTaskDelayUntil` **does not skip for free** — on a missed release it returns without delaying and a naive caller runs back-to-back; the wrapper detects the miss, **re-anchors** to the next future boundary, and counts the skip.
  - *resync*: execute once immediately, then re-anchor.
- **Phasing — three distinct mechanisms:** (1) *runnable order* within an activation (default; the bus serializes same-rate devices); (2) *divider offset* — requires divider ≥ 2 and `0 ≤ offset < divider`; dividers are permitted when the rate is absent from the task set **or when the VAL-019 co-location rule forces a rate mismatch**; (3) *time phase* — separate tasks with `phaseMs` offsets. A single **startup epoch** (latched once at gate release) defines phase zero.
- **Static allocation:** `configSUPPORT_STATIC_ALLOCATION=1`; all generated tasks/queues/buffers are static, sized from manifest `resources` (VAL-021). **RTE buffers are mandated internal DRAM** — no `EXT_RAM_ATTR` (PSRAM cache-coherency hazard); a linker-section assertion is generated.
- **Scoped heap guarantee:** *no dynamic allocation in generated or hand-written steady-state control paths.* Documented IDF exception list (NVS, console, UART driver) is excluded from the guarantee and covered by target-side allocation tracing in acceptance tests. (A heap watermark alone proves nothing — allocate/free pairs leave it unchanged.)
- **Numeric discipline:** `float32` and integer math only in `_calc.c` and RTE types (ESP32 FPU is single-precision; `float64` is soft-float). `int64` is permitted (used for time). Tasks touching `float` pay lazy FPU context save — flagged in the budget model.

### 6.2 RTE Data Semantics & Memory Model

**Sample contract** on every sample-group/event interface: `{value(s), qualityPerElement, sampleTimeUs, sequence}`. A default value with quality `INITIAL` is never a valid measurement. `sequence` advances **only on new acquisition**; a driver republishing a cached sample does not refresh `sampleTimeUs` — cached values age correctly by construction.

**Freshness** is a *consumer-port* property (`maxAgeMs`), evaluated at read time as `now − sampleTimeUs`.

**Access modes:** `implicit` (default) — one coherent snapshot per **runnable invocation** (not per task activation; separate runnables must not accidentally share a cache), cached for the remainder of the invocation; `explicit` — live read per call. Coherence is **per port**; cross-port coherence (two async sensors) is neither guaranteed nor claimed.

**Memory model (normative, per data class):**

| Data class | Mechanism |
|---|---|
| ≤32-bit scalar, single writer | `__atomic_load_n` / `__atomic_store_n` with explicit memory order (not bare `volatile`) |
| Sample group, same-core producer/consumer | **Short ESP-IDF critical section** around the bounded copy (target < 5 µs, measured — §8.3); no blocking calls inside |
| Sample group, cross-core (v1.1) | Seqlock with `__atomic` acquire/release ordering + Xtensa `memw` discipline; writer-priority analysis required before enabling |
| Event data (v1.1) | Static FreeRTOS queue with declared capacity, burst, service rate, and full-policy (never an unbounded ISR wait) |

The same-core seqlock was **deliberately rejected**: a higher-priority reader preempting a low-priority writer mid-write livelocks (documented Linux seqcount hazard). The critical-section copy is bounded, trivially correct, and cheaper to reason about.

### 6.3 Event-Driven Runnables (v1.1)
`trigger: "irq"` with edge/level config; dedicated **event tasks** (not combined periodic/event scheduling) in the first implementation; generated ISR wrapper defers via `vTaskNotifyGiveFromISR`/queue write. **IRAM safety covers the whole reachable graph:** code, referenced data, and interrupt registration flags — `IRAM_ATTR` on the wrapper alone is insufficient. v0.1/v1.0 are periodic-only.

### 6.4 Supervision — Four Levels, With Honest Limits

| Level | Mechanism | Detects | Cannot detect | Detection latency |
|---|---|---|---|---|
| L0 | Interrupt WDT + Task WDT (5 s, idle CPU0 checked; tasks subscribe **after the startup gate** and feed **once per activation**) | Software hang; a runnable that never returns (stops feeding) | Anything finer than its timeout | ≤ TWDT timeout |
| L1 | Generated wrapper: expected vs actual wake tick (+ jitter tolerance) | Missed/late activation | A task that never resumes at all | Next activation of that task |
| L2 | Completion vs release + deadline; optional per-runnable `wcetUs` budget | Deadline/execution overrun — **at completion only** | A runnable that never returns | At (non-)completion; L0 backstop |
| L3 | Hm: debounce/qualification of RTF events, driver `sequence` progression (wedged driver inside a live task), consumer stale policies, aggregation | Logical/alive/staleness failures | Its own death | Aggregation period |

**Hm is a mandatory TWDT subscriber; if Hm itself wedges, L1/L0 detect it, and Hm failure escalates only to L0.** Escalation ladder: report → degrade (per-policy scope) → safe state (actuator failsafes) → **EcuM controlled reset** (never a kernel panic) → L0 hardware reset. `Os_TaskOverrunCount` / skip / jitter counters are exposed to Hm and to the diagnostics interface.

### 6.5 Fault Qualification & Safe States
Hm implements Dem-inspired qualification: counter debounce (`failedCyclesToSet` / `passedCyclesToHeal`), fault memory, healing. **Actuator failsafes are triggerable, not declarative documentation:** every IoHwAb input port carries `maxAgeMs` + `holdCycles` + `failsafe` — generated into the IoHwAb runnable. The failsafe *value* is sourced from the provider port's declaration (the application states its intent); the IoHwAb *enforces* it. Failsafe values are per-port declarations; **100% duty is this demo's thermal policy, not a universal safe state.**

**Reset-window safety is a hardware requirement, stated as one:** between power-on and `EcuM_Startup`, and before PWM init, pin states are whatever the board's electrical design provides. The board manifest declares per-IO `idleLevel`; the **safe state at reset is guaranteed by external pull resistors and driver circuitry, not by generated code.** No software can close the bootloader window.

**Boot-loop protection:** N resets within M minutes (default 5/300 s, RTC-memory counter) forces terminal **SAFE_HALT**: outputs failed-safe, TWDT unsubscribed, bounded console forensics (reason, reset history, `esp_reset_reason()`), no further resets. Counter clears after 120 s of sustained RUN and on controlled shutdown.

### 6.6 NvM (v1.1)
RAM mirror per block with dirty tracking; `Nvm_WriteBlock` updates RAM only; NVS commits at `EcuM_Shutdown` (WriteAll), explicit save, or configurable bounded flush. Blocks carry version + CRC; CRC failure at boot → defaults + RTF-007 + quality INITIAL, never a hang. TWDT is disarmed around storage operations (sector erase can take tens of ms); **failsafes are applied before persistence**; a failed commit never blocks recovery. Wear budget estimated and reported.

### 6.7 Calibration (v1.1)
Parameters are **ports** (single wiring model); the generated **Cal** component is the provider, backed by NvM blocks. Transport: **lightweight XCP-on-UART** (CTO subset: CONNECT / GET_STATUS / DOWNLOAD / UPLOAD — no DAQ in v1.1), enabling standard automotive calibration tools. Values range-checked (DataConstr) on write.

### 6.8 MCAL Contracts
- Universal, **instance-based** APIs; AUTOSAR-style types (`Std_ReturnType`, `uint16/uint32` lengths — no `size_t`); `Std_Types.h`/`Platform_Types.h`/`Compiler.h` in the generated `Std` component.
- **Structured operational results** — `Mcal_ResultType { MCAL_OK, MCAL_TIMEOUT, MCAL_NACK, MCAL_ARB_LOST, MCAL_BUSY, MCAL_INVALID_ARG, MCAL_HW_FAIL }` — because drivers must distinguish timeout vs NACK to choose recovery and publish different comm faults. **Det reporting is separate** and reserved for API misuse; expected comm faults remain observable with DET compiled out. `esp_err_t` never crosses MCAL.
- Bus APIs are synchronous with **bounded, declared timeouts** (from driver execution contracts). Per-bus mutex is **uncontended in v1.0** (VAL-019 co-location) — `lockTimeoutMs` is retained in the schema for v1.1 and is **not charged** to the planning check (blocking that cannot occur).
- **Bus recovery** (9-clock SCL unstick + driver re-init) is owned by the MCAL, time-bounded, charged to the driver's `recovery.budgetUs`, and triggered after the declared consecutive-failure count with a cooldown.
- **Det/Log contract:** non-blocking static ring/queue, drained by a low-priority task, overflow counter incremented on full — never blocking, never from supervision paths or ISRs. (A NACKing driver that logs every cycle through a blocking UART would cause the very deadline misses it reports.)
- **Interface naming:** `Rte_Read_<Port>_<Element>()` — named after the **consumer's port**, never the provider.

### 6.9 Startup & Shutdown (EcuM)

```c
void app_main(void) { EcuM_Startup(); }
```

Sequence: boot-loop check → early log → `Port` init → bus init → PWM init (actuators at init values) → **device inits, failures recorded and policy-applied before any task runs** (a missing sensor never surprises Hm's first aggregation; *init OK ≠ first valid sample* — consumers see quality INITIAL until acquisition completes) → SWC inits → `Os_CreateTasks` → Hm init → `EcuM_Release`.

**Startup gate (normative):** "create suspended" does not exist in FreeRTOS and the create-then-suspend workaround races. Every application task's generated prologue blocks on a direct-to-task notification; `EcuM_Release` **latches the startup epoch once**, then notifies all tasks. TWDT subscription happens **after** the gate. No application runnable executes before the gate opens.

Shutdown: `EcuM_Shutdown` → actuators to failsafe **first** → (v1.1) bounded `Nvm_WriteAll` with TWDT disarmed → sleep/reset. `EcuM_MainFunction_500ms` executes in T500 and owns RUN ⇄ DEGRADED transitions and escalation actions.

---

### 6.10 ESP8266 backend qualification

Keep common RTE, driver and SWC contracts while implementing the smallest target-specific MCAL/Os/EcuM adapters. ESP8266 uses one core; derive priorities and interference from its own SDK, and validate the effective tick. Provide monotonic `int64` microsecond time (including wrap handling), bounded critical sections, startup notification/epoch behavior, skip/deadline checks, reset reasons and retained boot-loop accounting. Record whether any timer survives reset; uptime alone cannot measure a window spanning resets.

Map L0/watchdog behavior explicitly, including supervision of each required task, post-gate activation, one healthy-activation feed, and a terminal SAFE_HALT that remains stable without reset loops. ESP-IDF 5.2.3 TWDT subscription APIs and RTC annotations cannot simply be compiled for ESP8266. If an equivalent cannot be demonstrated, keep the target at bring-up status and record the gap.

Qualify I2C's actual timeout, clock stretching, error mapping and recovery on the selected SDK revision. The [ESP8266 I2C API](https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/api-reference/peripherals/i2c.html) uses command links and synchronous transfers; inspect allocation and timeout behavior before choosing it for a steady-state path. No dynamic allocation per display update, no unbounded retry, and no frame transfer inside an RTE critical section. A full 128×64 monochrome buffer is 1,024 bytes; payload plus ACK bits alone costs about 92.16 ms at 100 kHz (`1024 × 9 / 100000`). This is a calculated lower bound, not a measurement: use bounded chunks across activations and measure complete-frame latency separately.

Radio is disabled only for the initial OLED smoke baseline. WLAN/BT capability services are part of the current implementation track and must be selected through explicit ECU/backend capabilities, with startup, shutdown, reservation conflicts and fault behavior measured where supported. ESP8266 Bluetooth remains unsupported unless its SDK and hardware prove otherwise; no radio capability is inherited from ESP32. Arduino Adafruit/U8g2 sketches from R8 may be used as an independent hardware baseline, but their successful display output does not qualify Merlin's drivers or runtime.

---

## 7. Build System & Generated Project

### 7.1 Native ESP-IDF CMake (primary, approved)
`code/` is a native ESP-IDF project (`CMakeLists.txt`, `sdkconfig.defaults`, `idf.py`). Generated `sdkconfig.defaults` includes: `CONFIG_FREERTOS_HZ=1000`, static allocation, TWDT config, console settings. The generator **validates the effective sdkconfig** — writing defaults is insufficient when a full `sdkconfig` exists.

This is the ESP32 build profile. The planned ESP8266 output uses the candidate ESP8266 RTOS SDK v3.4/GCC 8.4.0 build, with its build commands/component dependencies established during qualification. ESP-IDF 5.2.3 is not an ESP8266 target backend. Each project generates one target tree; build jobs and SDK environments remain isolated. Arduino examples do not change the generated-project build contract.

### 7.2 No Source Copying
Driver and SWC sources are **referenced, never copied**, via `EXTRA_COMPONENT_DIRS` → repo `drivers/`, `handcode/`. `code/` contains only generated artifacts (`*_Cfg.c`, Rte, Os, EcuM, Hm, IoHwAb, app_main, build files), each carrying the GENERATED banner. Zero hand-code in the generated tree is a hard invariant.

### 7.3 Components & Enforcement
Every directory is an ESP-IDF component (`idf_component_register`). Dependency matrix (IDF **5.2.3** names — `driver`, not `esp_driver_i2c` which exists only from 5.3):

| Component | REQUIRES | PRIV_REQUIRES |
|---|---|---|
| Std | — | — |
| Mcal_I2c / Mcal_Pwm | Std, Det | driver |
| Drv_Bme280 | Std, Rte, Mcal_I2c | — |
| Rte | Std, Rte_Types | freertos, esp_timer |
| Swc_ClimateController | Rte, LibPid | — |

The second reference adds `Drv_Ssd1306` (public Std/Rte/MCAL I2c contract) and `Swc_DisplayDemo` (Rte only). The SDK dependency names in MCAL/Os/EcuM are backend-specific; verify them against the pinned ESP8266 SDK. Driver and SWC sources must not include Arduino `Wire`, display-library, or SDK headers. Include/symbol checks cover the current OLED/simulation paths; BME280 remains a deferred external transport until hardware is available.

`REQUIRES` is **defense-in-depth** (IDF auto-exposes freertos/HAL/log to all components; public deps propagate). The architecture boundary of §2.1 is enforced by **include/symbol linting plus negative compile tests**, e.g. `test/layering/neg_swc_includes_driver.c` — a file that includes `bme280.h` in SWC context, which CI must fail to build. `wizard.py audit` runs as a CMake pre-build target.

---

## 8. Verification & Acceptance

### 8.1 Generator Tests (built BEFORE the generator)
Checked-in `project.json` fixtures → **byte-exact golden output trees**, run in CI. The completed current-scope ESP32 reference tree (§11), using the simulated environmental provider, becomes **fixture #0** — the harness and comparator exist before the generator writes its first file. Plus: double-generate determinism job; template-version-pinned diff.

The qualified HW-364A tree becomes **fixture #1**. Each fixture pins its own SDK/profile and tests generation, build, layering and host behavior separately. Golden status requires the completed current-scope reference and board evidence; deferred external-device drivers are not fixture prerequisites, and neither current partial firmware nor an upstream Arduino example is already a golden reference.

### 8.2 Generated-Project Tests
- **Host (native):** ASW + Rte + mocked MCAL with a **fault-injection API** (`MockI2c_InjectFault(NACK|TIMEOUT|STUCK)`); driver `_calc.c` unit-tested against the datasheet worked example.
- **QEMU:** documented supported-peripheral profile (GPIO, UART, timers; **no I2C device models** — QEMU is a boot/integration check, not a driver bench; host mocks do not make firmware QEMU-runnable). The test SWC emits one **structured JSON line** per instance (health + supervision counters); the test asserts on the **parsed structure**, never the console transcript (dual-core log ordering is non-deterministic).

### 8.3 Acceptance Scenarios & Required Measurements

The table below retains the HW-394 climate scenarios. HW-364A adds TST-OLED-01…08 in [ECU support](docs/ecu-support.md#hw-364a-acceptance): identification/wiring, visible patterns, chunk scheduling, faults/recovery, initialization failure, snapshot ownership, allocation tracing, and target supervision. Common runtime scenarios run on each qualified backend; sensor/fan scenarios require those devices and are not replaced by an OLED test. Record evidence and unresolved measurements per ECU in [measurements](docs/measurements.md).

HW-364A requires host checks for frame packing/bounds, transfer progression and injected faults, plus physical OLED checks for orientation, full-screen patterns and changing counters. Serial counters prove transfer progress only; a visible panel check is required. The present Espressif ESP32 QEMU machine provides no HW-364A/OLED evidence.

| Scenario | Required evidence |
|---|---|
| Two identical sensors (0x76/0x77) | Independent config/state/outputs; distinct sequence counters |
| Sensor disconnected mid-run | Bounded NACK (≤ contract), DEGRADED after debounce, recovery attempt, other sensor unaffected |
| Cached value republished | `sampleTimeUs` unchanged; age advances; consumer sees stale |
| Bus stuck low | Bounded timeouts ×3, 9-clock recovery, cooldown, other tasks keep deadlines |
| Runnable overruns deadline | RTF-002 raised while TWDT stays silent |
| Driver init fails | Dependents follow `initPolicy`; system reaches DEGRADED, no boot loop |
| Concurrent sample access | Value + metadata coherent; critical-section duration measured on target |
| Skipped activation | Re-anchor, skip counter, PID `dt` correctness (activation delta) |
| Controlled reset forensics | 5 qualified deadline faults → failsafe-first reset → RTC counter + reason logged |
| Boot loop | 5 resets / 300 s → SAFE_HALT, not another reset |
| Calibration change (v1.1) | Effect without reboot; persists across controlled reset |
| NvM corruption (v1.1) | Defaults + RTF-007 + quality INITIAL; no hang |
| Identical inputs regenerated | Byte-identical tree |
| Generation interrupted | Previous output intact and buildable |

**Measurements that become manifest data** (`wcetUs`, `cpuUs`, `recovery.budgetUs`, RTA inputs): critical-section copy duration; per-state I2C transaction durations; T10 wake-jitter distribution over ≥10⁶ activations; recovery sequence duration; execution-time traces under simultaneous bus fault + logging.

---

## 9. Requirements, NFRs & Traceability

**Safety/security disclaimer (normative, repeated):** not developed per ISO 26262 / IEC 61508; no ASIL/SIL claim; AUTOSAR naming is methodological reuse only.

Requirement IDs `REQ-ARCH/GEN/RUN/DATA/BSW-nnn`, each mapped to design section, VAL/RTF, and test; full matrix maintained in `docs/traceability.md`. Sample rows:

| REQ | Requirement | Design | Validation | Test |
|---|---|---|---|---|
| REQ-ARCH-001 | SWC↔system via RTE only; driver↔HW via MCAL only | §2.1, §7.3 | — | negative compile tests + lint |
| REQ-ARCH-002 | Provider-agnostic ports; ASW includes no driver headers | §2.3, §4 | VAL-024/025 | fixture: swap provider, ASW unchanged |
| REQ-DATA-001 | Coherent per-port sample groups | §6.2 | — | TST-ACC-07 |
| REQ-DATA-002 | Freshness from `sampleTimeUs`, not publication | §6.2 | RTF-004 | TST-ACC-03 |
| REQ-RUN-001 | All timing values tick-representable | §6.1 | VAL-008 | generator fixture |
| REQ-RUN-002 | Deadline miss detectable independent of TWDT | §6.4 | RTF-002 | TST-ACC-05 |
| REQ-RUN-003 | No dynamic allocation in generated/hand-written steady-state control paths (scoped; IDF exception list documented) | §6.1 | — | target allocation tracing |
| REQ-BSW-001 | Init failures recorded & policy-applied before task release | §6.9 | RTF-005 | TST-ACC-06 |
| REQ-BSW-002 | Boot loop terminates in SAFE_HALT | §6.5 | — | TST-ACC-10 |
| REQ-GEN-001 | Byte-identical regeneration from identical lock + inputs | §3.3 | — | CI double-generate |

The traceability matrix also includes REQ-ECU-001…003, REQ-DISP-001…003, REQ-BOARD-001, REQ-DRV-001 and the 7L BswM, substitution and generator-closure rows for target selection, backend qualification, reproducibility, OLED composition, bounded transfers and board evidence. Software evidence is recorded; physical/electrical qualification remains explicitly deferred.

**NFRs:** generation ≤ 30 s for ≤ 50 device instances / 30 SWC instances / 8 tasks (generator capacity, not a promise that either MCU can host that workload); SDK/compiler pinned per ECU lock (ESP32: ESP-IDF 5.2.3; ESP8266 baseline: ESP8266 RTOS SDK v3.4/GCC 8.4.0, full qualification pending); Linux reference OS.

---

## 10. Release Plan

| Version | Content |
|---|---|
| **v0.1 — Reference Paths** (in progress, §11) | Complete the internal HW-394 runtime with simulated environmental data and the HW-364A OLED runtime with a qualified ESP8266 backend. **Separate board validation + measurements before either becomes a golden fixture; external device drivers remain deferred.** |
| **Schema freeze** | Measured numbers encoded into manifests; runtime-facing schemas (§4) frozen |
| **v1.0 — Generator MVP** | Reproduce both current-scope reference paths (fixtures #0/#1), target-specific MCAL, hardware-peripheral/acceleration, WLAN/BT and board capabilities, Os/EcuM/Det/Hm/Log, all applicable VAL/RTF rules and per-target CI. Unproven or deferred modules remain non-selectable. |
| **Deferred external-device track** | Add exact external drivers, wiring/calibration/electrical evidence, manifests and fixtures after the required hardware is available; BME280 is the first example. |
| **v1.1** | Async bus transfers (lifting VAL-019) with synchronous facade; queued/event ports; IRQ runnables (dedicated event tasks, IRAM whole-graph); Cal + XCP-on-UART; NvM; overlays; Twai; BswM-style mode management; GPTimer release option; PlatformIO (unpinned, best-effort) |
| **v1.2** | ESP32-S3/C3 SoC profiles; ULP; secure boot / flash encryption via guarded expert flow; E2E protection on Twai; Eth, Sdio; crypto + entropy gating |
| **v2.0** | Broader catalog; GUI configurator on the same JSON model |

---

## 11. Reference Implementation (v0.1) — Status

`v01-reference/` contains the hand-built ESP32 internal climate path: MCAL, simulated environmental provider, RTE, PID/controller, PWM fan path, static tasks/startup gate, supervision, structured fake-sensor output, host tests and a provider-agnostic host OLED/display slice. It builds and has booted in QEMU and on HW-394. The default smoke image uses simulated data; the exact external BME280 transport and physical measurements remain deferred. `v01-hw364a-reference/` is the hand-built ESP8266/HW-364A companion; it builds, flashes, boots and transfers repeated frames on the connected unit. Non-physical backend and capability evidence is recorded; remaining electrical/physical measurements remain deferred. The intended completed tree is:

```
v01-reference/
├── CMakeLists.txt · sdkconfig.defaults
├── main/main.c                      # void app_main(void) { EcuM_Startup(); }
├── test/layering/neg_swc_includes_driver.c   # must FAIL to compile (boundary proof)
└── components/
    Std/  Rte/ (Rte_Type, Rte_Interfaces, Rte, Rte_Cfg, Rte_ClimateController)
    Mcal_Port/  Mcal_I2c/  Mcal_Pwm/  Os/
    Drv_Bme280/ (provisional simulation/contract path; external transport deferred)
    LibPid/  Swc_ClimateController/  IoHwAb/  Hm/  Log/  EcuM/
```

It must demonstrate: instance model, sample contract + per-element quality, freshness from `sampleTimeUs`, critical-section RTE transfer, startup gate + epoch, skip re-anchor + activation delta, structured `Mcal_ResultType` + 9-clock recovery with cooldown, triggerable IoHwAb failsafe, L1/L2 in the task wrapper, Hm debounce, EcuM SAFE_HALT + RTC forensics.

**Two normative patches to apply before hardware validation** (identified during final consolidation): (1) TWDT subscription moved to **after** the startup gate, with **one feed per activation** in the wrapper; (2) SAFE_HALT **unsubscribes** from the TWDT (version-robust; does not rely on blocked-task exemption).

When the current-scope runtime is complete and measured, this tree becomes **golden fixture #0**. Planned `v01-hw364a-reference/` becomes **fixture #1** after its own backend, OLED, capability and supervision acceptance. Deferred external-device qualification is not required for these fixtures. Neither reference authorizes starting the generator before the fixture harness. [Measurements](docs/measurements.md) records the limited evidence currently available.

---

## 12. Open Points (final remaining)

1. **HW-394 board manifest population** — schema and draft manifest exist; physical header/pull-up/onboard-device verification remains pending.
2. **v1.1 UART multiplexing** — XCP tuning and log on one framed UART vs. a dedicated second UART.
3. **v1.1 async driver API shape** — preferred: async core with a synchronous bounded facade; alternative: completion callbacks. Decide at v1.1 design.
4. **HW-364A board identity** — processor identity, 2 MB flash, OLED wiring/controller ACK and GPIO14/GPIO12 are recorded; fitted pull-ups, header exposure and reset behavior remain. Resolve reference D-label ambiguity using GPIO numbers.
5. **ESP8266 backend qualification** — the v3.4/GCC 8.4.0 pin, native build/flash/boot and repeated OLED transfers are recorded; §6.10 watchdog, timing, reset-history and SAFE_HALT proof remain.
6. **OLED execution contract** — measure bounded chunk/command time, recovery and full-frame latency under load; freeze static buffer ownership and scheduling from evidence.
7. **Internal capability qualification** — complete the available MCU/board MCAL, hardware-peripheral/acceleration, WLAN/BT capability and board-driver evidence; unsupported services must remain explicitly non-selectable.
8. **Deferred external devices** — exact BME280 and other external-device drivers, wiring, calibration, electrical tests and related fixtures wait for the required hardware.

---

## Appendix A — Decision Record (adopted scope; qualification still required)

| # | Decision |
|---|---|
| 1 | Native ESP-IDF CMake for ESP32; separate native ESP8266 backend pending qualification; PlatformIO out of v1.0 |
| 2 | Type/instance split for drivers and SWCs; context-pointer calling convention |
| 3 | Provider-agnostic port interfaces with semver; binding structural w/ subtyping |
| 4 | Sample contract: per-element quality, `sampleTimeUs` int64 µs, `sequence` uint32 wrap-safe |
| 5 | Freshness = consumer `maxAgeMs` evaluated from `sampleTimeUs`; activation rate ≠ acquisition rate |
| 6 | Same-core sample transfer = short critical section (seqlock rejected: livelock hazard); cross-core seqlock v1.1 with acquire/release + `memw` |
| 7 | Startup gate via task notifications; single epoch latch; no create-suspended |
| 8 | Four-level supervision with declared blind spots; Hm failure escalates only to L0 |
| 9 | TWDT: subscribe after gate, feed per activation, SAFE_HALT unsubscribes |
| 10 | EcuM state machine incl. SAFE_HALT boot-loop terminal state + RTC forensics |
| 11 | Execution contracts closed by derivation (VAL-023); retries on next activation |
| 12 | Co-location rule VAL-019 for v1.0; async state-machine drivers lift it in v1.1 |
| 13 | `Mcal_ResultType` structured; Det separate, misuse-only; Det/Log non-blocking |
| 14 | Reset-window safe state = hardware guarantee (board `idleLevel` declared) |
| 15 | v1.0 scope = the two current-scope ECU reference paths and their proven capabilities, generated; deferred external devices remain non-selectable; harness before generator |
| 16 | Lock records and pins, never overrides; `--frozen` + explicit `resolve` |
| 17 | RTA (not RMS) for VAL-012; Info severity; explicitly not a proof |
| 18 | Cal = XCP-on-UART CTO subset (v1.1); parameter ports in schemas from day one |
| 19 | Generic ESP8266 ECU + HW-364A board + reusable SSD1306 driver added by owner, 2026-09-19; documentation first, Luna Code implementation next |
| 20 | ECU profiles have separate SDK/capability/measurement records; one ECU per generated project; no cross-ECU transport added |
| 21 | OLED data passes through RTE → driver → MCAL; bounded chunks, static frame ownership, physical visual acceptance |
| 22 | ECU, board and device driver are independent selections; HW-364A expands to an ordinary OLED instance and fixed physical reservations; generic ESP8266 has no default devices |

---

**Immediate next actions for Luna Code:** keep the current software/QEMU/fixture checks green → perform the deferred Section 3 physical pass when HW-394 is connected → freeze qualified manifest values → defer BME280 and other exact external-device qualification until the required hardware is available. Phase 2–6 non-physical implementation and local evidence are complete for the current-scope reference compositions.
