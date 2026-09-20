# CLAUDE.md

This file provides repository guidance for coding agents, including Luna Code.

## What this is

Merlin is a **code generator**, not firmware. It reads JSON manifests and emits
a target-specific native SDK project for ESP32 or ESP8266 with an AUTOSAR-style
layered stack (MCAL / drivers / RTE / BSW / ASW) already wired together.

Two documents are normative and outrank anything inferred from the tree:

- **[PROJECT_DEFINITION.md](PROJECT_DEFINITION.md)** — the approved design,
  v2.2.0, owner-requested ECU scope amendment. Section numbers below refer to
  that document. If code and the document disagree, the document wins until amended.
- **[NEXT_STEPS.md](NEXT_STEPS.md)** — the phased plan and where work currently
  sits. Update its status table when a phase gate passes.

**Current state:** toolchain/CLI skeleton, host tests and two hand-built
references exist — ESP32 `v01-reference/` and ESP8266 `v01-hw364a-reference/`.
Both build; the ESP32 image passed QEMU and HW-394 boot, and the HW-364A image
drives the soldered OLED on hardware with a startup gate, deadline detection
and watchdog supervision. Environmental sensing is still a deterministic
simulation provider: no external sensor has been wired. The generator and the
frozen schemas do not exist. Read [NEXT_STEPS.md](NEXT_STEPS.md) for what is
proved versus implemented — this paragraph goes stale faster than that table.

[ECU support](docs/ecu-support.md) is the target/board/driver contract and Luna
handoff; [measurements](docs/measurements.md) records actual evidence. ECU target,
board profile and device driver are separate selections. Generic ESP8266 selects
drivers/wiring explicitly; HW-364A auto-adds one standard SSD1306 instance and
its verified pins/address. Never fork the OLED driver for that board.

## Commands

The installer, `check-env`, host tests and Python environment tests exist.
Other wizard subcommands are placeholders returning exit 2. The commands below
mix available tooling and planned interfaces (§3.3/§5.1); generator/fixture
commands remain future work. The wizard has no ESP8266 target option yet; the
hand-built reference is built with its SDK directly, as below. Preserve
established command spelling when extending them.

```bash
./install.sh [--dry-run]              # idempotent; pins IDF 5.2.3 + QEMU + python deps
python scripts/wizard/cli.py check-env
python scripts/wizard/cli.py validate project.json
python scripts/wizard/cli.py generate [--frozen] [--non-interactive]
python scripts/wizard/cli.py audit    # lock hashes vs working tree; also a CMake pre-build target
cd code && idf.py build               # the generated project
./test/run_tests.sh                   # host tests: plain gcc + assert, no framework, no board
pytest scripts/                       # generator tests, incl. golden-fixture comparison

# The two hand-built references, each against its own pinned SDK.
( . ~/esp/esp-idf/export.sh && cd v01-reference && idf.py build )
( cd v01-hw364a-reference && IDF_PATH=~/esp/ESP8266_RTOS_SDK \
    PATH=~/esp/xtensa-lx106-elf/bin:~/esp/esp8266-venv/bin:$PATH make )
```

Build both after touching anything under `v01-reference/components/`: the two
SDKs disagree about which APIs exist, so a shared MCAL source can compile for
one target and fail to resolve a component for the other.

Exit codes are part of the contract: `0` ok, `1` validation error, `2`
environment error. A validation failure is never exit 0 with a warning on
stdout.

Run a single generator test with `pytest scripts/tests/test_allocate.py -k
sticky`. Host C tests follow the one-binary-per-unit pattern — `run_tests.sh`
compiles each unit with its test file under `-fsanitize=address,undefined` and
runs it; to run one, invoke that `gcc` line directly.

## Hard invariants

These are the things that quietly break the project if violated. Most of them
are cheap to check and expensive to discover late.

**`code/` is generated. Never edit it.** Zero hand-code in the generated tree is
a structural invariant, not a preference — every file carries a GENERATED
banner, and the next `generate` replaces the tree. If something in `code/` is
wrong, the template or the manifest is wrong.

**Driver and SWC sources are referenced, never copied.** `EXTRA_COMPONENT_DIRS`
points at `drivers/` and `handcode/` in the repo. A generator that copies
sources into `code/` has broken the regeneration boundary.

**ESP32 uses ESP-IDF 5.2.3.** The component is `driver` — `esp_driver_i2c`
and friends exist only from 5.3 and must not appear anywhere. When checking IDF
API shapes, check them against 5.2.3, not against latest.

**ESP8266 is a separate backend.** Qualify/pin its native ESP8266 RTOS SDK and
compiler before implementation acceptance. The current installer/toolchain.env
is ESP32-only. Reuse portable driver/RTE/SWC contracts; adapt MCAL/Os/EcuM for
the target's timing, allocation, watchdog and reset semantics. Keep SDK
environments separate. Generic ESP8266 and HW-364A use the same backend.

**Board defaults are ordinary instances plus physical constraints.** Expand
HW-364A's OLED instance once before dependency resolution/allocation, auto-select
I2c, reserve its pins/address, and report them in review/lock output. Re-loading
must be idempotent. Conflicting physical wiring is an error, not an override.
Disabling display software does not free soldered pins/address. Other I2C
devices may share the compatible bus at distinct addresses and within budgets.

**Generation is deterministic.** Sort every iteration at the boundary where
output order is decided, never emit a timestamp by default, keep Jinja2 pinned.
CI generates twice and byte-diffs. A dict iteration order that happens to be
stable today is not determinism.

**Allocations are sticky (§5.4).** `allocate` and `add` may add assignments;
they may never move one that already exists. The board may be soldered.
Reassignment without an explicit release is VAL-015, an Error.

**The lock records, never overrides (§3.2).** `project.json` is intent;
`project.lock` is history. A code path where the lock silently changes what gets
generated is a bug, no matter how convenient.

**VAL/RTF identifiers are stable.** `VAL-001`…`VAL-026`, `RTF-001`…`RTF-007` are
referenced from the traceability matrix, from `acknowledgedWarnings` entries in
user projects, and from tests. Add new IDs at the end; never renumber, never
reuse.

VAL-026 rejects incompatible/unqualified ECU/SDK/driver capabilities. Draft
schemaVersion 2.1.0 examples remain in the specification; Phase 3 revises/freezes
2.2.0 with independent ECU, SDK and board selection. Do not create schema or
manifest files during a documentation-only task.

## Runtime rules the generated code must honour

Getting these wrong produces firmware that looks fine and is subtly untrustworthy.

ESP32-specific mechanisms below require a demonstrated ESP8266 equivalent
(PROJECT_DEFINITION §6.10); shared semantics do not imply shared SDK APIs.

- **`sampleTimeUs` is not a publication time.** A driver republishing a cached
  reading leaves the timestamp and the `sequence` counter alone. That is the
  whole mechanism by which stale data ages correctly — refreshing either one on
  republish silently destroys staleness detection everywhere downstream.
- **Freshness belongs to the consumer.** `maxAgeMs` is a property of the
  consuming port, evaluated at read time as `now − sampleTimeUs`. Providers do
  not decide who considers them stale.
- **Implicit access is per runnable invocation**, not per task activation. Two
  runnables in the same task must not share a snapshot cache.
- **Same-core sample transfer is a short critical section** around a bounded
  copy, with no blocking call inside. The seqlock was evaluated and rejected: a
  high-priority reader preempting a low-priority writer livelocks. Do not
  reintroduce it for the same-core case.
- **TWDT: subscribe after the startup gate, feed once per activation, and
  unsubscribe in SAFE_HALT.** All three are §11 normative patches. Subscribing
  before the gate arms a watchdog against tasks that are deliberately blocked.
- **The startup gate is a direct-to-task notification.** FreeRTOS has no
  create-suspended, and create-then-suspend races. `EcuM_Release` latches the
  startup epoch exactly once, then notifies.
- **`skip` overrun policy must re-anchor.** `vTaskDelayUntil` returns without
  delaying on a missed release, so a naive wrapper runs back-to-back activations
  and calls it "skipping". Detect the miss, re-anchor to the next strictly
  future boundary, count it.
- **Static allocation only in steady-state control paths.** RTE buffers go in
  internal DRAM — never `EXT_RAM_ATTR`, the PSRAM cache-coherency hazard is
  real. Generate the linker-section assertion.
- **`float32` and integers only.** The ESP32 FPU is single-precision; `float64`
  is soft-float and does not belong in a control path. `int64` is fine and is
  what time uses.
- **`esp_err_t` never crosses MCAL.** MCAL returns `Mcal_ResultType` so a driver
  can tell TIMEOUT from NACK and pick a recovery. Det is separate and reserves
  itself for API misuse — comm faults must stay observable with Det compiled out.
- **Det and Log never block.** Static ring drained by a low-priority task,
  overflow counter on full. A NACKing driver that logs through a blocking UART
  causes the deadline misses it is reporting.
- **OLED transfer is bounded work.** Static, immutable active-frame ownership;
  bounded pending storage; chunks across activations with bounded retry/recovery.
  No I2C transfer in an RTE critical section. Do not copy BME280 transaction
  budgets to an entire display frame. Count completed frame transfers separately
  from physical visible-output acceptance.

## File organisation

One concern per file, split aggressively, generic mechanism never in the same
file as project-specific policy.

For driver and SWC components this has a specific shape that matters more here
than in most projects: **every driver splits into a pure arithmetic half and a
transport half.** `bme280_calc.c` holds the Bosch compensation and is tested on
the host against the datasheet's worked example; `bme280.c` talks to the chip.
Hardware validation is expensive and deferred, so the host-tested half is the
only part with real evidence behind it — put anything that can be computed
without a bus on that side of the line.

Apply the same split to SSD1306 packing/geometry and command transport. Hardware
pins live in board/instance configuration; neither SSD1306 nor BME280 contains
HW-364A-specific conditionals. Raw ESP8266 driver compatibility is qualified per
combination; ESP32-only MCAL peripherals must never become silent stubs.

Before writing a helper, look for one that exists. The repository is meant to
accumulate reusable driver arithmetic; a second debouncer or a second PID is a
regression.

## Conventions

- Generated C: C11, `-Wall -Wextra`, warning-free. Generator: Python ≥3.10.
- AUTOSAR-style types in MCAL and RTE — `Std_ReturnType`, `uint16`/`uint32`
  lengths, not `size_t`.
- `Rte_Read_<Port>_<Element>()` is named after the **consumer's** port. Never
  after the provider — that is the whole point of provider-agnostic interfaces.
- Runnables take an instance context: `void ClimateController_Run(ClimateController_CtxType* self)`.
  Drivers take instance handles: `void Bme280_MainFunction_High(Bme280_InstanceType inst)`.
  Single-instance types use the same convention as multi-instance ones — no
  special case.
- **"Port" is overloaded.** The MCAL `Port` module configures pins; an RTE
  *port* is a component interface endpoint. Always qualify which one in prose,
  comments and identifiers.
- Manifest and schema files are RFC 8259 JSON — no comments, no trailing commas.
- `schemaVersion` appears in every manifest and is checked, not assumed.

## Claims not to make

The project deliberately declines several claims, and generated banners, docs
and commit messages must not quietly reintroduce them:

- It is **not AUTOSAR conformant**. Naming and methodology are reused; per-module
  fidelity is declared in §2.2 and is "name-inspired" for most of them.
- It is **not developed to ISO 26262 or IEC 61508** and carries no ASIL/SIL claim.
- Configuration is **link-time** (compiled C). The phrase "post-build" is not
  used in this project.
- VAL-012's response-time analysis is a **planning check, not a schedulability
  proof**, and is Info severity for that reason.
- The **native-mux 40 MHz threshold is project policy**, conservative, and not a
  silicon limit. Label it as such wherever it appears.
- QEMU has **no I2C device models**. It is a boot and integration check; it
  cannot validate a driver.

## Hardware facts worth not rediscovering

The following bullets describe ESP32/WROOM; do not use them as an ESP8266 pin map.

- GPIO6–11 are the on-module SPI flash on WROOM-32. GPIO37/38 are not bonded
  out. Neither is routable, ever.
- GPIO34–39 are input-only with no internal pull resistors — an output or pull
  configuration on them is VAL-002, an Error, and an external pull is the only
  fix.
- IO0/2/5/12/15 are strapping pins. Usable, but VAL-003 warns and the warning
  needs an explicit acknowledgment.
- I2C on ESP32 is matrix-routed only — there is no native-mux constraint on it.
  Its real constraint is electrical: rise time ≈ 0.847·R·C against 1000 ns at
  100 kHz and 300 ns at 400 kHz. Above 100 kHz, external pull-ups are required.
- ADC2 is shared with the radio.

For HW-364A, use the sourced configuration and verification checklist in
[board evidence](docs/ecu-support.md#board-evidence). Numeric GPIOs are
authoritative, not D-label aliases. Flash size, pull-ups and module identity
remain physical verification items. Generic ESP8266 assumes no onboard OLED
and requires explicit wiring; no HW-394 limits or SDK resources are inherited.

## Testing posture

Host tests are plain `gcc` plus `assert()` with ASan and UBSan — no framework,
no fixtures. Add a unit test for any new arithmetic; it is the only evidence
that exists before a board is on the bench.

The generator's own regression is golden fixtures: a checked-in `project.json`
and the exact tree it must produce, compared byte for byte. **The fixture
harness is built before the generator** (§8.1) — that ordering is deliberate and
is Phase 4 in NEXT_STEPS. Do not start emitting files before there is something
that can tell you the output changed.

Fixture #0 is the completed HW-394 reference; fixture #1 is the qualified
HW-364A OLED reference. Add generic ESP8266 and board-default/conflict cases.
Real OLED patterns, error recovery and ESP8266 supervision are physical tests;
the existing ESP32 QEMU smoke is no substitute. Preserve evidence levels in
documentation: planned, implemented, host-tested, and hardware-qualified.
