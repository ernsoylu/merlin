# Merlin — Phased Plan

Working plan of record. Section references (§) point at
[PROJECT_DEFINITION.md](PROJECT_DEFINITION.md), which is normative; this file
says only **in what order** and **against what evidence**.

Update the status table when a gate passes. A phase is not done because its
deliverables exist — it is done when its **gate** holds.

## Current scope decision — 2026-09-19

Available hardware is limited to development boards and the connected
ESP8266/HW-364A OLED. External device-driver work that needs an exact device
or additional bench hardware is deliberately deferred: BME280 transport,
external-sensor wiring, calibration, electrical fault qualification and their
device-specific manifests/fixtures are not current blockers.

The active path is the internal runtime and board/backend work that can be
proved now: Std/Rte/Os/EcuM/Hm/Det/Log, MCAL services, board-specific and
hardware-peripheral drivers, WLAN/BT capability services where the ECU
supports them, the connected SSD1306/OLED path, resource/capability validation,
host tests and devboard evidence. The existing BME280 fake path remains a
simulation provider only.

## Status

| Phase | Name | Gate | Status |
|-------|------|------|--------|
| 0 | Repository skeleton & toolchain | `check-env` green on a clean machine | **complete** |
| 0E | ESP8266 backend qualification | generic ECU SDK/compiler pinned; build and runtime feasibility recorded | **complete; full behavioral/watchdog qualification is Phase 2** |
| 1 | v0.1 reference firmware | internal runtime, simulated-provider path, HW-364A OLED path, host/layering tests and target boots | **complete for current scope; external device drivers deferred** |
| 2 | Available-hardware validation & measurement | internal/runtime, radio-capability, hardware-peripheral and connected-OLED evidence | **non-physical gate complete for current scope; Section 3 physical-equipment evidence deferred to the final pass** |
| 3 | Schema freeze | measured internal/board/backend numbers and modular ECU/driver model; §4 schema 2.2.0 frozen | **active pre-freeze; current-scope schemas/manifests exist with explicit unverified physical fields** |
| 4 | Fixture harness | harness reproduces a diff for a deliberate one-byte change | **complete; byte-exact comparison, deliberate-diff, atomic-output and lock tests pass** |
| 5 | Generator MVP | current-scope fixtures regenerate byte-identically; generic ESP8266 and board-default selection work | **current-scope implementation complete; generic/reference renderer boundary remains explicit** |
| 6 | v1.0 hardening & release | CI green incl. determinism + negative compile tests | **software gate complete locally; CI workflow and hardware qualification remain release gates** |
| 7 | v1.1 | — | **started; Packages 7A–7E async-bus, event-port, IRQ-task, and cross-core sample contracts complete** |
| 8 | Deferred external-device qualification | exact external drivers, wiring, calibration and physical acceptance | **deferred until hardware is available** |
| 9 | v2.0 | — | planned |

**Current reality check (2026-09-19).** The ESP32 climate runtime now builds with
ESP-IDF 5.2.3, passes the host suite and explicit BME280/OLED layering checks,
passes parsed QEMU structured-output startup, and has booted on HW-394 with
verified 4 MB flash writes. The HW-394 image currently uses deterministic fake
sensors by default; external BME280 wiring and measured control behavior are
deliberately deferred until the device is available. A same-day HW-394 session
(devkit connected, no BME280
wired) found and fixed two real bugs: the RTC boot-loop counter used
`RTC_DATA_ATTR`, which ESP-IDF documents as surviving deep sleep only, not a
plain `esp_restart()` (fixed with `RTC_NOINIT_ATTR` plus a 300 s window that
didn't exist before at all); and `EcuM_RecordDeadlineFault`'s
`resetRequested` flag was set but never acted on anywhere (fixed: fan forced
to failsafe, reset logged, then `esp_restart()`). Re-verified end to end: 5
controlled resets -> `SAFE_HALT` on boot 6, zero TWDT panics. A new
`CONFIG_MERLIN_INJECT_SLOW_T500` Kconfig knob and the real (non-fake) I2C path
against an unpopulated bus together evidenced TST-ACC-05/06/08/09/10 without
needing a physical sensor; TST-ACC-01/02/04 remain deferred until a BME280 is
wired. The new fake transport disconnect knobs also exercised the mid-run
isolation/recovery path: ambient recovered to `READY`, enclosure stayed
`READY`, EcuM returned to `RUN`, and the recovery counter reached 1 with zero
deadline skips or fault markers; this is simulation evidence only. The session
also simulated persistent ambient loss: the cached sample became stale and the
fan reached `1000‰` failsafe while the enclosure remained healthy. The session
also found that ordinary diagnostic `printf` logging alone
(no fault at all) can self-trigger the controlled-reset/boot-loop path via
likely priority inversion on shared blocking stdio -- root-caused and fixed by
routing task-path output through the existing `Log_Ring` and a dedicated static
low-priority drain task; a 125 s no-injection HW-394 run passed with one
heartbeat, zero deadline faults/skips, and zero panic/watchdog/reset markers.
See
[measurements](docs/measurements.md) for full detail. The native ESP8266 RTOS SDK v3.4 HW-364A reference now builds,
hash-verifies its flash writes, boots the identified ESP8266EX/2 MB unit, and
repeatedly transfers frames to the OLED at GPIO14/GPIO12, address 0x3C. A
2026-09-19 re-run of the unmodified reference found the OLED init NACKing on
every reset (root cause: ESP8266 RTOS SDK v3.4 NACKs the first I2C transaction
after `i2c_param_config`, independent of target address); a fix landed in the
shared `Mcal_I2c_Init` and was re-verified across 6/6 resets plus a 30 s
sustained run, and the owner visually confirmed a correct moving pattern on
the physical panel, passing TST-OLED-02. Host, layering, and both
reference-path checks are complete. A same-day follow-up added a real startup
gate, per-activation skip/deadline detection, TWDT feed-once-per-activation,
and software fault injection to `v01-hw364a-reference`, then verified
TST-OLED-03 (per-chunk I2C timing, avg 2445us/max 2529us), TST-OLED-04
(5-NACK burst -> 1 bounded recovery -> correct resumed redraw), TST-OLED-05
(forced init failure -> DEGRADED, no boot loop), TST-OLED-06 (new host-only
two-instance independence test), and TST-OLED-07 (heap/stack held flat across
1120+ transfers) on the physically connected unit. TST-OLED-08 is now
qualified for the retained reset path: skip/deadline/watchdog-silence and the
five-count `SAFE_HALT` decision passed with both injected software resets and
tight RTS-pin resets after the ESP8266 storage moved from `RTC_DATA_ATTR` to
`RTC_NOINIT_ATTR`; no display task ran in halt.
TST-OLED-01's pull-up/electrical record and TIMEOUT/stuck-bus fault injection
still need bench instrumentation this session didn't have. HW-394's full §8.3
scenario suite was not attempted this session (that board was not connected).
See [measurements](docs/measurements.md).

### USB-only execution policy

All Phase 2 software, QEMU, schema, fixture and generator work proceeds before
the physical-equipment pass. Section 3 physical items are intentionally not
closed by simulation: pull-up/reset-window facts, electrical I2C timeout and
stuck-bus behavior, recovery timing under a real fault, and PWM output/load
qualification remain explicitly `unverified` or `deferred`. QEMU may qualify
portable runtime behavior and fake-provider fault policy, but never ESP8266,
SSD1306, real I2C or board electrical limits. Ask the owner to connect HW-394
only when an ESP32 target build or target-specific evidence is the next
unblocked check; until then HW-364A remains the only connected target.

### Current software boundary

The current implementation validates the 2.2.0 project model, expands board
defaults, resolves typed connections, checks the lock, and reproduces the two
checked-in reference trees byte-for-byte. Non-golden projects receive
deterministic project metadata and a generated configuration translation unit;
the hand-built reference runtime remains the intentional current-scope output
boundary. The headless `configure`, `add` and `remove` commands revalidate every
edit. All 26 VAL entry points, warning acknowledgements, provider-swap/NFR,
interrupted-render and traceability checks are now covered locally.

**Owner expansion:** generic ESP8266 is a second ECU target; HW-364A is its board
profile; SSD1306 is a reusable driver selected automatically by that board with
its bus/pin/address reservations. Generic ESP8266 selects devices explicitly.
[ECU support](docs/ecu-support.md) defines compatibility and acceptance. The
hand-built reference now covers both documented paths; schema/generator work
remains later phases.

Phase 0's existing completion marker applies only to the original ESP32 setup.
Phase 0E records the isolated ESP8266 RTOS SDK v3.4/GCC 8.4.0 environment and
native HW-364A runtime baseline; it does not claim full qualification.
Phases 2–6 now proceed using available devboards, internal services,
board-specific drivers and the connected OLED. External device-driver
qualification is deferred to Phase 8 and is not a prerequisite for schema,
fixture or generator work.

---

## Phase 0 — Repository skeleton & toolchain

**Goal.** A clean Linux machine goes from `git clone` to a verified toolchain
without reading a wiki page.

### Deliverables

| Path | Content |
|------|---------|
| `install.sh` | idempotent, `--dry-run`, non-root (sudo only for system packages), ends by invoking `check-env` |
| `requirements.txt` | `jinja2`, `jsonschema`, `questionary`, `pytest` — all pinned to exact versions |
| `scripts/wizard/cli.py` | registered §5.1 surface; current-scope `new`, `configure`, `add`, `remove`, `generate`, `validate`, `resolve`, `allocate`, `rte`, `audit` implemented; device security remains Phase 8 |
| `scripts/wizard/core/` | model, validation, allocation checks, RTE resolution, deterministic rendering and lock audit |
| `test/run_tests.sh` | host C test driver — plain gcc, `-std=c11 -Wall -Wextra -fsanitize=address,undefined`, one binary per unit |
| `.github/workflows/ci.yml` | lint + pytest + `run_tests.sh`; jobs added per phase |
| `.gitignore` | `code/`? **no** — generated output is committed for fixture diffing; ignore `build/`, `sdkconfig` (not `sdkconfig.defaults`), `__pycache__`, `.venv` |
| `docs/traceability.md` | the §9 matrix, seeded with the ten sample rows |

### Tasks

1. Pin the toolchain in one place and read it from there: ESP-IDF **5.2.3**,
   Espressif QEMU, Python ≥3.10. `install.sh` and `check-env` must agree because
   they share the constant, not because someone kept them in sync.
2. `check-env` verifies: `idf.py` present and reporting 5.2.3, xtensa toolchain
   on PATH, QEMU present, Python version, every pinned package importable at the
   pinned version. It prints what it found, not just pass/fail — a version
   mismatch has to be diagnosable from the output alone.
3. Decide and document the `code/` commit policy now. Committing generated
   output makes every regeneration a reviewable diff, which is the cheapest
   determinism check there is. That is the recommendation.

### Gate

`./install.sh && python scripts/wizard/cli.py check-env` succeeds on a machine
that has never seen the project, and a second `./install.sh` changes nothing.

---

## Phase 0E — ESP8266 backend qualification (baseline complete)

Use generic ESP8266 as the backend unit and HW-364A as its first physical test
board. Start with the candidate ESP8266 RTOS SDK v3.4 and GCC 8.4.0 toolchain;
verify the tag/archives and pin exact artifacts separately from ESP-IDF 5.2.3.
Prove basic build/flash/monitor and
audit §6.10: timing, one-core task model, static allocation, watchdog task
coverage, retained reset history and bounded I2C. Document unresolved gaps.
Do not promise ESP32-specific APIs or use its QEMU machine as ESP8266 evidence.

Confirm board/module identity, actual flash capacity, numeric OLED wiring,
pull-ups and reset arrangement. An upstream Arduino display smoke may establish
a hardware baseline, but cannot substitute for Merlin driver/runtime acceptance.

**Gate:** baseline passed. The environment is pinned to ESP8266 RTOS SDK v3.4
commit `89a3f254b63819035f65d9c5dcdae8864f1a6a8a` and GCC 8.4.0; the native
HW-364A target builds, flashes with verified hashes, boots, initializes the
documented OLED, and transfers repeated frames. Full behavioral proof belongs
to Phase 2. Unsupported services remain non-selectable.

---

## Phase 1 — v0.1 reference firmware (hand-built)

**Goal.** Prove the runtime contract by hand, on two concrete ECU configurations,
before any generator exists. Everything v1.0 later generates must first exist
here as code a person wrote and understood.

The current reference composition is the climate-demo shape of §4.4, but uses
the deterministic environmental simulation provider: two simulated sensor
instances, one `ClimateController` SWC instance, one PWM fan through IoHwAb,
three tasks (T10/T100/T500), and no radio in the smoke image. Exact external
BME280 transport is deferred. The second configuration uses generic
ESP8266 services with HW-364A board wiring and a reusable SSD1306 driver/display
SWC. Its schedule and budgets must be established independently.

### Deliverables

`v01-reference/` as laid out in §11:

```
v01-reference/
├── CMakeLists.txt · sdkconfig.defaults
├── main/main.c                                # void app_main(void) { EcuM_Startup(); }
├── test/layering/neg_swc_includes_driver.c    # must FAIL to compile
└── components/
    Std/          Std_Types.h, Platform_Types.h, Compiler.h
    Rte/          Rte_Type, Rte_Interfaces, Rte, Rte_Cfg, Rte_ClimateController
    Mcal_Port/  Mcal_I2c/  Mcal_Pwm/  Os/
    Drv_Bme280/   dual-instance, start-check-read, integer Bosch compensation
    LibPid/  Swc_ClimateController/  IoHwAb/  Hm/  Log/  EcuM/
```

The planned `v01-hw364a-reference/` adds the ESP8266 MCAL/runtime backend,
`Drv_Ssd1306`, `Swc_DisplayDemo` and their RTE/health binding. Board wiring lives
in configuration, never in the reusable driver. Generic ESP8266 supports explicit
SSD1306 and BME280 selections once their combinations are qualified; no onboard
devices are presumed. See [driver compatibility](docs/ecu-support.md#driver-compatibility).

### Build order

Bottom-up, because each step adds exactly one thing that can be wrong.

1. **Std + Rte types.** `Std_Types.h`, `Platform_Types.h`, `Compiler.h`, and the
   normative metadata types: `sampleTimeUs` as `int64` µs from
   `esp_timer_get_time()`, `sequence` as wrap-safe `uint32`, per-element quality.
2. **MCAL.** `Mcal_Port`, `Mcal_I2c`, `Mcal_Pwm`. Instance-based APIs,
   `Mcal_ResultType` out, `esp_err_t` never out. Bounded declared timeouts. The
   9-clock SCL unstick lives here, time-bounded, with the cooldown.
3. **Os.** The task wrapper is the load-bearing piece: release wait → record
   actual wake tick → runnables in declared order → deadline check → TWDT feed.
   L1 and L2 are both inside it. Implement `skip` with the re-anchor, and write
   the test that proves back-to-back activation does not happen.
4. **Rte.** Per-port storage, the critical-section bounded copy, implicit
   snapshot cached per *runnable invocation*, freshness evaluated at read.
5. **Environmental simulation provider.** Keep the existing fake BME280-shaped
   transport for runtime, recovery, stale-data and failsafe tests. Do not make
   exact BME280 transport, wiring or physical qualification a prerequisite for
   the current reference gate; that work belongs to Phase 8.
6. **LibPid + Swc_ClimateController.** The SWC includes no driver header and no
   ESP-IDF header — that is what `neg_swc_includes_driver.c` exists to prove.
   PID `dt` comes from the activation delta, not from the nominal period.
7. **IoHwAb.** The fan instance, with `maxAgeMs` / `holdCycles` / failsafe
   generated into the runnable so the failsafe is *executable*.
8. **Hm.** Debounce (`failedCyclesToSet` / `passedCyclesToHeal`), `sequence`
   progression checking, RTF qualification, aggregation. Mandatory TWDT
   subscriber.
9. **EcuM.** Startup sequence per §6.9, the notification startup gate with the
   single epoch latch, RUN ⇄ DEGRADED, controlled reset, SAFE_HALT with RTC
   boot-loop counter and forensics.
10. **Log/Det.** Static ring, low-priority drain task, overflow counter. Never
    blocking, never called from supervision paths or ISRs.

### ESP8266 and OLED implementation track

1. Implement target-specific MCAL/Os/EcuM services with portable public
   contracts and a generic explicit-wiring configuration. The HW-364A baseline
   is now present in `v01-hw364a-reference/`.
2. Implement SSD1306 packing/bounds as pure host-testable logic and command/data
   transfer through MCAL I2c. Keep frame ownership static and coherent across
   bounded chunks; measure transfer time before setting deadlines/timeouts.
3. Wire `DisplayDemo.DisplayOut → onboardOled.Frame` and its health report.
   Test visible patterns/counters and error handling on HW-364A. Board hardware
   defaults and application bindings remain separate concerns.
4. Implement and qualify board/backend services that are available without
   external devices: UART, I2C, PWM/GPT/watchdog, WLAN/BT capability selection
   where supported, and target-specific hardware-peripheral or acceleration
   paths. Unsupported services must fail validation rather than inherit from
   another ECU.
5. Prove generic selection without an OLED is valid, and that the board-specific
   reference uses the same driver/backend rather than a HW-364A fork. Keep
   external BME280 selection visible but deferred/non-selectable until Phase 8.

The existing ESP32 climate work can proceed independently. Neither track may
claim the other track's timing, peripheral or watchdog evidence.

### Apply the two §11 patches as you write, not afterwards

1. TWDT subscription happens **after** the startup gate; the wrapper feeds **once
   per activation**.
2. SAFE_HALT **unsubscribes** from the TWDT rather than relying on blocked-task
   exemption, which is not version-robust.

These are the ESP32 mechanisms. Demonstrate equivalent task coverage and stable
SAFE_HALT on ESP8266 using its qualified watchdog backend.

### Host tests (`test/run_tests.sh`)

Every one of these runs without a board and is the only evidence that exists
until Phase 2.

| Unit | Asserts |
|------|---------|
| `bme280_calc` | integer compensation vs the datasheet's floating-point reference across the ADC range, agreeing to within one output LSB |
| `pid` | `dt` from activation delta; output clamping at `outMin`/`outMax`; integral windup bounded |
| `rte` | snapshot coherence; `sampleTimeUs` unchanged on cached republish; `sequence` advances only on new acquisition; stale detection at the boundary |
| `os_wrapper` | skip re-anchors to a strictly future boundary and counts once; deadline miss raises RTF-002; jitter tolerance honoured |
| `hm_debounce` | fault sets after N, heals after M, no set on N−1 |
| `mcal_result` | TIMEOUT and NACK are distinguishable at the driver; comm faults survive Det compiled out |
| `ssd1306` | packing/bounds, initialization/transfer order, immutable active frame, bounded pending frame, instance independence, injected failures and recovery progression |

Plus the layering negative test: CI must observe `neg_swc_includes_driver.c`
**failing** to compile. A negative test that silently stops being compiled is
worse than no test — assert on the failure explicitly.

Make the forbidden include itself cause rejection, and assert the expected
diagnostic. Remove unrelated undefined/uninitialized behavior from the negative
case and pair it with a valid positive compile control. Cover OLED/SDK includes
as well as BME280; otherwise a generic compiler failure can give a false pass.

### QEMU

Boot check only: GPIO, UART, timers. No I2C device model, so the BME280 path
runs against a fake. The test SWC emits one structured JSON line per instance;
assert on the parsed structure, never on the console transcript.

This integration check is ESP32-specific. The current startup runs the static
task gate and fake BME280 transport, then emits structured records. ESP8266 requires
its own physical boot/runtime evidence; no OLED emulator is assumed.

### Gate

Both complete reference paths build under their pinned SDKs; host tests pass
under ASan/UBSan; negative compile cases fail for the intended boundary; ESP32
QEMU and physical ESP8266 runs demonstrate startup and structured health output.
The HW-364A run reports `health:1`, increasing completed frame sequences, and
zero transfer failures. A pair of constant JSON lines does not satisfy this
gate. Physical device acceptance and measurements follow in Phase 2.

---

## Phase 2 — Available-hardware validation & measurement campaign

**Goal.** Qualify what can be built and measured with the available devboards
and connected HW-364A OLED. Record numbers separately per ECU/backend/board for
internal services, board-specific drivers, radio capability paths, hardware
peripherals/acceleration and the OLED. External-device qualification is not a
Phase 2 gate.

### Bring-up order

Each step adds one thing that can be wrong, so a failure localises itself.

1. **Board and ECU inventory.** Confirm module identity, flash, exposed pins,
   reset behavior, clocks, SDK/backend and available peripheral controllers.
2. **Internal runtime.** Port/Dio, UART, I2C, PWM/GPT, watchdog, startup gate,
   timing, logging, supervision and failsafe behavior with no external device.
3. **Hardware-peripheral/acceleration paths.** Implement and test the target's
   available PWM/LEDC, ADC/RMT/GPT/SPI or equivalent services; record unsupported
   capabilities explicitly per ECU.
4. **Radio services.** Add WLAN and BT capability contracts/backends where the
   selected ECU supports them; validate core reservations, ADC2 conflicts,
   startup/shutdown and unsupported-service rejection. ESP8266 BT remains
   unsupported unless the selected SDK proves otherwise.
5. **Full internal loop.** Runtime → board services → actuator/display, with
   supervision armed and structured evidence. Use simulated environmental data
   where the application needs a provider.

### Phase 2 implementation record and remaining physical boundary

1. **Internal BSW/MCAL services — active.** DIO now has a standalone
   validation/native-GPIO contract with host evidence, the UART contract has a
   native SDK backend with host validation, and the ESP8266 OLED path reuses
   the shared OS release/deadline state machine, and the HW-364A task now
   feeds the watchdog through an MCAL adapter, and OLED timing now uses the
   shared MCAL GPT microsecond timebase. The existing PWM adapter now also
   compiles against the ESP8266 native PWM API with host contract coverage.
   Port and I2c now validate their arguments with host coverage, and the new
   `Mcal_Mcu` adapter owns reset and reset-reason mapping for both backends --
   EcuM and the HW-364A main no longer call `esp_restart`/`esp_reset_reason`
   directly. Retained (RTC) boot-loop storage is still SDK-specific in BSW and
   needs a hardware session to qualify on ESP8266. EcuM is now split into a
   portable state machine and the ESP32 climate startup, and both references
   share one host-tested boot-loop decision (`EcuM_EvaluateBootLoop`) instead
   of the three divergent copies they had -- the HW-364A copy had no window
   and ignored the reset reason. Complete target validation, plus the
   remaining Os/EcuM/Det/Log/Rte integration and capability reporting. The
   HW-364A display task now consumes the shared Hm sequence debounce and
   records RTF-002/003/006; a two-cycle missed-completion injection raised Hm
   at `failed:2` and healed on the next completion. Portable/software fault
   escalation is covered locally; target physical qualification remains
   deferred.
2. **Hardware-peripheral drivers — active.** The ESP8266 PWM adapter is the
   first target-specific peripheral path; it is build-validated but not flashed
   or driven on an unconnected output. The ESP8266 ADC wrapper now has target
   evidence: TOUT initializes/reads on HW-364A, while VDD is explicitly
   unsupported under the board's PHY calibration. The SPI capability boundary
   now exposes HSPI only: CSPI is
   reserved by flash, and HSPI's fixed GPIO12–15 mapping overlaps the OLED.
   The minimal reservation checker now rejects HSPI/OLED GPIO collisions and
   requires an explicit compatible key for shared I2C. Full schema/CLI
   allocation is closed in Phase 5; RMT remains explicitly unsupported on
   ESP8266 until an SDK-backed path exists.
3. **WLAN/BT services — capability boundary active.** The radio contract now
   exposes explicit WLAN/BT capability bits and rejects unsupported selection.
   `Mcal_Wlan` now selects its stack/event-loop bring-up per SDK -- ESP8266
   keeps `tcpip_adapter` and the legacy loop, ESP-IDF 5.2.3 uses `esp_netif`
   and the default loop, which is what the removed component had broken.
   The capability bits are no longer empty on every target: a bit means the
   build carries a backend, so WLAN is declared on both ESP targets and BT on
   none, and `Mcal_Wlan_Init` now claims it through `Mcal_Radio_Select` rather
   than trusting the caller -- host evidence builds the same test twice, with
   and without the capability. The reservation checker reserves ESP32 ADC2 for
   a selected radio; the allocator now also reserves `CORE1` for ESP32 radio
   claims. Target startup fault reporting is now structured as `RTF-005-WLAN`;
   portable EcuM startup/shutdown and Hm fault paths are covered locally;
   target physical qualification remains open.
   The ESP8266 native WLAN init/start/stop hook now compiles and passed a
   corrected opt-in HW-364A hardware smoke (`init=0`, `start=0`, `stop=0`)
   using strict SDK results; the earlier `WIFI_MODE_NULL`/invalid-argument
   acceptance was removed. The default OLED image remains disabled. Runtime
   resource reservations and Hm fault qualification are covered by the current
   validation/QEMU/host evidence; board measurements remain in the final
   Section 3 pass.
   ESP8266 WLAN is still
   target work;
   ESP8266 Bluetooth is unsupported unless proven by its SDK. ESP32 WLAN/BT
   remains capability-gated rather than assumed.
4. **HW-specific drivers — current-scope complete.** Keep pins, reset levels, board defaults and
   soldered-device reservations in board profiles; add only thin target
   adapters where the SDK or silicon requires them. The HW-364A SSD1306 path is
   the available concrete driver and remains in scope.
5. **Evidence — current-scope complete.** Host, QEMU, SDK-build and available
   HW-364A evidence are recorded in the capability matrix. The remaining
   measurements are deliberately physical Section 3 items.

### HW-364A bring-up order

Identify processor/module/flash and verify wiring → probe the documented OLED
bus → establish visible clear/fill/corner/checkerboard patterns → run changing
counter through SWC/RTE/driver/MCAL → qualify chunk timing, faults and supervision.
Run [TST-OLED-01…08](docs/ecu-support.md#hw-364a-acceptance), recording visual
confirmation separately from ACK/serial evidence. External BME280 support is
not part of this gate; the OLED and internal/runtime evidence stand on their own.

### Acceptance scenarios (§8.3)

Each needs recorded evidence, not a "looks right":

- internal service instances: independent config, state, outputs and counters
- board driver disconnected/fault-injected mid-run: bounded error, recovery,
  **other services unaffected**
- cached simulated value: `sampleTimeUs` unchanged, age advances, consumer
  sees stale and actuator/display policy remains safe
- bus fault injection: bounded timeouts, 9-clock recovery, cooldown, other tasks
  still meet deadlines
- WLAN/BT capability selection: supported services initialize and unsupported
  services are rejected without contaminating another ECU's capability set
- runnable overruns its deadline: RTF-002 raised **while the TWDT stays silent**
- driver init fails: dependents follow `initPolicy`, system reaches DEGRADED, no
  boot loop
- concurrent sample access: value and metadata coherent
- skipped activation: re-anchor, skip counter, PID `dt` correct
- controlled reset forensics: five qualified deadline faults → failsafe first →
  reset → RTC counter and reason logged
- boot loop: five resets in 300 s → SAFE_HALT, not a sixth reset

### Measurements that become manifest data

These are the numbers Phase 3 consumes. Record method and conditions alongside
each value — a WCET without its conditions is a rumour.

| Measurement | Feeds |
|-------------|-------|
| critical-section copy duration | §6.2 target < 5 µs; RTE budget |
| per-state I2C transaction duration | `transactionTimeoutMs`, `cpuUs`, `maxElapsedMs` |
| T10 wake-jitter distribution, ≥10⁶ activations | `wakeJitterToleranceTicks`; justifies or refutes tick release |
| recovery sequence duration | `recovery.budgetUs` |
| execution traces under simultaneous bus fault **and** logging | `wcetUs` worst case, RTA inputs |
| heap allocate/free trace in steady state | REQ-RUN-003 (a watermark proves nothing — pairs cancel) |
| OLED command/chunk duration and complete-frame latency | display timeout, task period and transfer budget; include bus speed/CPU load |
| ESP8266 wake jitter, critical sections, stack/static buffer usage | separate runtime and resource budgets; no copied ESP32 measurements |

### Gate

Every applicable scenario passes with evidence recorded in `docs/measurements.md`;
every number above exists with its conditions for the relevant ECU. Generic
ESP8266 support lists exactly which device combinations were tested. No target
becomes qualified solely from another board's measurements.

### Blocked on

Open point §12.1 — the HW-394 board manifest has to be populated from a physical
board: header availability per pin, fitted pull-up values, onboard devices, and
the per-IO electrical `idleLevel` for the reset window. HW-364A and generic
ESP8266 configurations require their own module/wiring facts and flash checks
(§12.5–7). Do this on the boards, not from an unverified sales listing. These
facts constrain final qualification but do not block the current internal,
radio, hardware-peripheral or OLED implementation track. Exact external device
drivers remain deferred until their hardware is available.

---

## Phase 3 — Schema freeze

**Goal.** Turn measurements into manifest data and stop moving the runtime-facing
schemas.

### Tasks

1. Write the §4 manifests for the current scope: internal service/capability
   manifests, the simulated environmental provider,
   `handcode/climatecontroller/climatecontroller.json`,
   `project.json`, `soc/esp32.json`, `modules/esp32-wroom-32.json`,
   `devkits/devkit-hw394.json`. Replace every executable placeholder number with
   a Phase 2 measurement, with a margin policy that is written down rather than
   intuited; deferred Section 3 physical fields remain explicitly unverified
   and make the affected capability non-selectable.
   Add the generic ESP8266 SoC/module/wiring profiles, HW-364A board defaults,
   reusable SSD1306 driver manifest, WLAN/BT capability manifests,
   hardware-peripheral capability manifests, `MonochromeFrame`, display SWC and
   second reference composition. Add driver/ECU compatibility and qualification
   state. Keep external BME280 manifests deferred/non-selectable.
   Separate `target.ecu` and SDK from board selection (§4.6); record physical
   constraints independently from configurable defaults. Do not hardcode OLED
   pins in the device manifest or force an OLED onto generic ESP8266 projects.
2. Write JSON Schema (Draft 2020-12) for each manifest kind into
   `scripts/schemas/`: project, lock, interface, driver, swc, soc, module, board,
   overlay. Implement `print-schema` against these files so the schemas have
   exactly one home.
3. Verify VAL-023 closure holds with the *measured* numbers:
   `maxElapsedMs ≥ transactionTimeoutMs + recovery.budgetUs`. If it does not
   close, the contract is wrong and this is the moment to find out.
4. Ship the standard interface catalog: `EnvironmentalData`, `HealthReport`,
   `PwmDutyCycle`, `DioLevel`, `PidParams`, `MonochromeFrame`.
5. Tag the revised schemas `2.2.0` frozen. Explain the draft `target.espIdf` to
   ECU/SDK selection change; no released 2.1.0 schema migration is required.
   After this, a change to a runtime-facing
   schema is a version bump with a migration note, not an edit.

### Gate

Every current-scope manifest validates against its schema; every executable
number traces to a Phase 2 measurement; VAL-023 closes; `print-schema` emits all
current-scope schemas. Deferred external-device manifests are not required for
this gate.

---

## Phase 4 — Fixture harness (before the generator)

**Goal.** Build the thing that can tell you the output changed, *before*
anything writes output. §8.1 is explicit about this ordering and it is worth
defending: a generator built before its comparator gets its regressions
discovered by hand.

### Deliverables

| Path | Content |
|------|---------|
| `scripts/tests/fixtures/00-climate-demo/project.json` | the §4.4 composition using the simulated environmental provider |
| `scripts/tests/fixtures/00-climate-demo/expected/` | byte-exact expected tree — **initially a copy of `v01-reference/`** |
| `scripts/tests/fixtures/01-hw364a-oled-demo/` | project and qualified ESP8266/OLED expected tree, from `v01-hw364a-reference/` |
| `scripts/tests/harness.py` | generate into a temp dir, compare byte-for-byte, report a readable per-file diff |
| `scripts/tests/test_fixtures.py` | pytest driver over every fixture directory |

### Tasks

1. The comparator reports a *useful* diff: which files differ, which exist on
   one side only, and a unified diff per differing file. A boolean is useless at
   3am.
2. Self-test the harness before trusting it: flip one byte in `expected/` and
   confirm it reports exactly that byte. A harness that always passes is the
   worst possible outcome of this phase.
3. Normalisation policy, decided now and written down: what may legitimately
   differ between runs (nothing, by design) and what is stripped before
   comparison (nothing, by preference). Every escape hatch added here weakens
   REQ-GEN-001.
4. Wire the double-generate determinism job into CI now, even though it has
   nothing to generate yet.

### Gate

`pytest scripts/tests/test_fixtures.py` runs, and a deliberate one-byte change
in `expected/` produces a diff naming that file and that byte.

---

## Phase 5 — Generator MVP (v1.0)

**Goal.** Reproduce the current-scope fixtures byte-for-byte, support generic
ESP8266 with explicit capability/driver selection, and resolve HW-364A defaults
through the same model. Selectable scope is exactly the qualified
internal/backend/OLED/radio combinations
(Appendix A, decisions 15 and 22).

Build the pipeline in dependency order; each stage gets tests before the next
starts.

### 5.1 Model layer — `core/model.py`

Load and merge `project.json`, manifests, and the four hardware tiers
(SoC ∧ module ∧ board ∧ overlay). Expand board-default device instances once,
then apply the §3.2 precedence chain — explicit
project value > instance config > manifest default > interface default — and
**report every conflict it resolves**. Silent precedence is how a generator
becomes untrustworthy. Physical onboard-device wiring remains a validation
constraint even when the application disables its software use.

### 5.2 Validation — `core/validate.py`

All twenty-six VAL rules with their declared severities. Keep one rule per
function, named for its ID, so the traceability matrix maps to the code
mechanically. Notable ones:

- **VAL-012 (RTA)** — iterate `R_i = C_i + B_i + Σ_{j∈hp(i)} ⌈R_i/T_j⌉·C_j` to a
  fixed point against `D_i`. `B = 0` in v1.0 under the VAL-019 co-location rule.
  Info severity. Report it as a planning check and never call it a proof.
- **VAL-018** — compute I2C rise time as ≈ 0.847·R·C from the declared pull-up
  and bus capacitance, against 1000 ns at 100 kHz and 300 ns at 400 kHz.
- **VAL-023** — *derive* the closure rather than trusting the declaration.
- **VAL-025** — structural binding: provider range ⊆ consumer range, exact unit
  match, no implicit conversion.
- **VAL-026** — target/SDK compatibility and qualified capabilities, including
  ESP8266 core/peripheral restrictions and board-default reservations.

Warning acknowledgment is per ID **and** per object, bound to the config-hash of
the affected subtree, so an ack stops applying when the thing it excused changes.

### 5.3 Allocation — `core/allocate.py`

A target-specific resource table, not a pin list: only controllers/timers and
channels actually exposed by the chosen backend. ESP32 resource kinds include
LEDC groups, ADC units and RMT; those are not assumed on ESP8266. Board devices
reserve pins and addresses before optional external devices are allocated.

- exclusive by default; compatible sharing permitted where the configuration
  matches (PWM channels on a timer at the same frequency and resolution)
- the sticky invariant is the hard part: existing assignments are constraints.
  Test it directly — allocate, add a device, assert nothing moved.
- hard blocks from the tier intersection; native-mux rule above the 40 MHz
  policy threshold, labelled as policy

### 5.4 RTE wiring — `core/rte.py`

Auto-bind by interface and version; list unresolved and ambiguous bindings
explicitly rather than guessing. Fan-out is allowed (each consumer gets its own
snapshot); fan-in is not (multiple providers into one requirer is an error).

### 5.5 Generation — `core/generate.py` + `scripts/templates/`

- render to a staging directory, validate, then replace atomically — an
  interrupted generation leaves the previous output intact and buildable
- sort every iteration at the boundary; no timestamps; Jinja2 pinned
- GENERATED banner on every emitted file
- `EXTRA_COMPONENT_DIRS` references `drivers/` and `handcode/`; nothing is copied
- emit `sdkconfig.defaults` **and validate the effective sdkconfig** — writing
  defaults does nothing when a full `sdkconfig` already exists

### 5.6 Lock — `core/lock.py`

Record generator and template versions, per-source content hashes,
soc/module/board/overlay versions, schema versions, ECU and pinned SDK/toolchain,
resolved board instances and wiring, driver compatibility, dependencies and
accepted warnings. Implement `--frozen` (reject drift),
`resolve` (refresh explicitly), and `audit` (hashes vs working tree), and wire
`audit` as a CMake pre-build target.

### 5.7 CLI and interactive steps

`new` walks the current-scope S1→S9 path: it asks for project metadata and one
of the two qualified reference compositions, then displays the derived target,
modules, devices, buses, allocation, schedule and bindings before creation.
`--non-interactive` selects the same composition deterministically. The full
VAL report, diff against the previous `project.json`, lock preview and final
generation remain available headless. Arbitrary unqualified composition design
is outside the v1.0 generator boundary.

### Non-physical Phase 2 closure record — 2026-09-20

The USB-only portion of Phase 2 is closed for the current scope. Host tests,
ESP32 QEMU scenarios, ESP8266/HW-364A SDK compilation, capability/resource
validation, radio rejection paths, OLED fault-policy simulation and generated
reference builds are recorded in `docs/measurements.md` and
`docs/traceability.md`. This record does not close Section 3 physical facts.
Those remain a final-pass checklist: HW-364A electrical/reset measurements,
HW-394 board/header/pull-up/reset measurements, real bus-fault timing, PWM
output/load and long-run target measurements.

### Gate

`generate` on the current-scope fixtures reproduces both reference trees, which
build under their respective SDKs and pass their Phase 1 tests. Generic
ESP8266 has no implicit OLED; HW-364A adds the normal SSD1306 instance and
reservations exactly once. Pin/address conflicts, unsupported capabilities and
deferred external drivers fail validation or are reported non-selectable.

**Current evidence:** both fixtures reproduce byte-for-byte; generic ESP8266 has
no implicit OLED; HW-364A expands one OLED with its reservations; lock audit,
frozen generation, warning acceptance, target conflicts and schema validation
pass locally. The generated arbitrary-runtime boundary is intentionally limited
to the two qualified reference renderings.

---

## Phase 6 — v1.0 hardening & release

**Goal.** Make the guarantees checkable by CI rather than by discipline.

### Tasks

1. **Determinism job** — generate twice, byte-diff, fail on any difference.
2. **Negative compile tests in CI** — assert the layering violations *fail*.
   Add the include/symbol lint alongside, since `REQUIRES` cannot prove the
   boundary on its own.
3. **Provider-swap fixture** (REQ-ARCH-002) — replace the simulated
   environmental provider with a second provider of `EnvironmentalData` and
   assert the ASW output is unchanged. Keep this provider internal/simulated;
   do not pull deferred external-device work into the current fixture set.
4. **Interrupted-generation test** — kill mid-render, assert the previous output
   is intact and still builds.
5. **Fixture set** beyond #0: single instance; three instances; a VAL error case
   per error-severity rule; a warning-acknowledgment case; a sticky-allocation
   case; fixture #1 HW-364A defaults; generic ESP8266 with no OLED; explicit
   SSD1306/radio/peripheral combinations; compatible shared bus; duplicate OLED
   address; pin conflict; disabled onboard-device reservations; board change
   without implicit relocation; wrong SDK/core/peripheral rejection; deferred
   external-driver rejection.
6. **NFR check** — generation ≤ 30 s at 50 device instances / 30 SWC instances /
   8 tasks. Generate that fixture and time it.
7. **Complete `docs/traceability.md`** — every REQ mapped to design section,
   VAL/RTF and test, with no unmapped rows.
8. **Documentation pass** — README and CLAUDE.md reconciled with what actually
   shipped; every "not started" in this file resolved.

### Gate

CI green across per-target build, host tests, generator tests, determinism,
negative compile tests and the NFR timing job. Hardware qualification is recorded
separately and required for every advertised supported combination. Tag v1.0.

**Local evidence for this gate:** host C tests, 38 Python tests, negative
layering compile checks, provider swap, interrupted generation, fixture byte
diffs, both reference SDK builds, ESP32 QEMU scenario assertions and the NFR
fixture pass. CI remains the repository gate; Section 3 hardware qualification
is deliberately separate and deferred.

---

## Phase 7 — v1.1

Each item is independently shippable; the ordering below is by how much it
unblocks.

### Package 7A — bounded async-bus contract slice — 2026-09-20

Implemented the fixed-capacity `Mcal_I2cAsync` queue and bounded synchronous
facade in the portable MCAL contract. It supports submit, one-step service,
completion callbacks, cancellation on budget expiry and queue-full rejection;
it allocates no heap memory. Host coverage passes queue order, capacity,
completion, cancellation, timeout and invalid-request cases. The ESP-IDF 5.2.3
MCAL object/archive also compiles. This package does not yet run a FreeRTOS
bus worker or claim target contention/timing evidence; those belong to Package
7B. Physical qualification remains deferred.

### Package 7B — async-bus task adapter — 2026-09-20

Complete for the current software-only scope. The existing static `Os` task
wrapper can now schedule `Mcal_I2cAsync_ServiceTask` directly as its runnable;
each activation services at most one queued transfer, so the task period is the
declared service rate and the queue capacity remains bounded. Host coverage
proves the adapter services one request and safely ignores a null context. No
FreeRTOS target timing, contention, or physical qualification is claimed.

### Package 7C — queued/event port contract — 2026-09-20

Complete for the current software-only scope. `Rte_EventQueue` is a fixed,
heap-free four-entry queue with declared burst, service-rate and full-policy
configuration. Push is nonblocking; service drains no more than the declared
rate per activation. ESP32 uses `xQueueCreateStatic`; host tests use identical
ring semantics. Drop counters and both drop-newest/drop-oldest policies are
covered. ISR integration, event-task generation, and physical qualification
remain deferred to the following package and final hardware pass.

### Package 7D — IRQ runnable task bridge — 2026-09-20

Complete for the current software-only scope. `Os_CreateStaticEventTask` creates
a static FreeRTOS event task that blocks on a direct notification, while
`Os_NotifyEventFromIsr` performs only the bounded ISR-safe notification and
requests a context switch. The runnable executes in task context through the
host-tested dispatch guard. No interrupt registration, peripheral ISR, whole
reachable-graph IRAM audit, target timing, or physical qualification is claimed.

### Package 7E — bounded cross-core sample transfer — 2026-09-20

Complete for the current software-only scope. The environmental sample now has
an optional cross-core seqlock path using acquire/release `__atomic` operations,
Xtensa `memw` barriers, a single-writer contract, and three bounded reader
retries. Readers return failure rather than spinning indefinitely while a writer
is active. Host tests cover null arguments, an in-progress write, coherent
publication/readback, and an even completed version. Target timing and
multi-core contention evidence remain deferred.

1. **Async bus transfers with a synchronous bounded facade** — lifts VAL-019,
   the co-location rule, which is the largest artificial constraint in v1.0.
   Reintroduces real mutex contention, so `lockTimeoutMs` starts being charged
   to the planning check. Settles open point §12.3.
2. **Queued/event ports** — static FreeRTOS queues with declared capacity,
   burst, service rate and full-policy. Never an unbounded ISR wait.
3. **IRQ runnables** — `trigger: "irq"`, dedicated event tasks, ISR wrapper
   deferring via `vTaskNotifyGiveFromISR`. **IRAM safety covers the whole
   reachable graph** — code, referenced data and interrupt registration flags.
   `IRAM_ATTR` on the wrapper alone is not enough and is the classic way to get
   a crash that only happens during a flash write.
4. **Cross-core sample transfer** — seqlock with `__atomic` acquire/release and
   Xtensa `memw` discipline. Writer-priority analysis before it is enabled, not
   after.
5. **NvM** — RAM mirror, dirty tracking, NVS commit at shutdown, version + CRC,
   CRC failure → defaults + RTF-007 + quality INITIAL. TWDT disarmed around
   storage; failsafes applied *before* persistence.
6. **Cal + XCP-on-UART** (CTO subset: CONNECT / GET_STATUS / DOWNLOAD / UPLOAD,
   no DAQ). Parameter ports already exist in the schemas, so this replaces the
   const provider without touching a single SWC signature — which is the test
   that the port model was right. Settles open point §12.2.
7. **Overlays** — per-project wiring deltas as the fourth tier.
8. **Twai**, **Icu/PCNT subset**, **GPTimer release option**, **BswM-style mode
   management**, **PlatformIO output** (explicitly unpinned, best-effort,
   outside the reproducibility guarantee).
9. Settle open point §12.4 — substitute-value policy and who owns substitution
   on INVALID.

---

## Phase 8 — Deferred external-device qualification

This phase starts only when the exact external hardware is available. It is
deliberately after the internal runtime, board/backend, radio, hardware-
peripheral, schema, fixture and generator work.

1. Attach and identify the target external device(s); record wiring, power,
   pull-ups, address straps and module markings.
2. Implement the reusable device driver through the existing Std/Rte/MCAL
   contracts; do not fork a board-specific driver unless the hardware truly
   requires a target adapter.
3. Add host fault tests, target timing, electrical fault/recovery evidence,
   manifests, compatibility entries and current-scope fixtures only after the
   device passes its acceptance gate.
4. Re-run shared-bus, resource allocation, generator and regression checks.

BME280 is the first deferred example. Its current fake transport, recovery,
stale-data and failsafe tests remain useful contract coverage but do not claim
physical driver support.

## Phase 9 — v1.2/v2.0 platform expansion

Broader driver and SWC catalog. GUI configurator over the same JSON model —
the model is the product; the GUI is a second front end onto it, and if it needs
schema changes to work, the schema was wrong.

---

## Standing rules for every phase

- **Evidence before encoding.** No measured number enters a manifest before it
  has been measured, and no phase gate passes on inspection alone.
- **Harness before generator.** Anything that emits files gets its comparator
  first.
- **Qualified references are the generator baseline.** Compare each target with
  its completed, measured reference; current partial code does not override the
  normative design. Keep fixtures #0/#1 and explicit generic compositions distinct.
- **ECU, board, driver stay separate.** HW-364A auto-selects the reusable OLED
  driver and fixed resources; generic ESP8266 never inherits those devices/pins.
- **Stable identifiers.** VAL and RTF IDs are append-only. They are referenced
  from user projects, tests and the traceability matrix.
- **Declared limitations stay declared.** RTA is not a proof, the 40 MHz
  threshold is policy, QEMU cannot validate a driver, and the reset-window safe
  state is a hardware guarantee. Each phase that touches one of these restates
  it rather than quietly dropping it.
