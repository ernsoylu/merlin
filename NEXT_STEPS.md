# Merlin — Phased Plan

Working plan of record. Section references (§) point at
[PROJECT_DEFINITION.md](PROJECT_DEFINITION.md), which is normative; this file
says only **in what order** and **against what evidence**.

Update the status table when a gate passes. A phase is not done because its
deliverables exist — it is done when its **gate** holds.

## Status

| Phase | Name | Gate | Status |
|-------|------|------|--------|
| 0 | Repository skeleton & toolchain | `check-env` green on a clean machine | **complete** |
| 1 | v0.1 reference firmware | builds warning-free, host tests pass, QEMU boots | **in progress** |
| 2 | Hardware validation & measurement | all §8.3 scenarios pass on HW-394, numbers recorded | **not started** |
| 3 | Schema freeze | measured numbers in manifests, §4 schemas tagged frozen | **not started** |
| 4 | Fixture harness | harness reproduces a diff for a deliberate one-byte change | **not started** |
| 5 | Generator MVP | fixture #0 regenerates byte-identical | **not started** |
| 6 | v1.0 hardening & release | CI green incl. determinism + negative compile tests | **not started** |
| 7 | v1.1 | — | planned |
| 8 | v1.2 | — | planned |
| 9 | v2.0 | — | planned |

**Current reality check.** The repository holds documentation only. §11
describes `v01-reference/` as delivered, but that tree is not here — Phase 1
builds it in this repository. Nothing in Phases 3–6 can start against
hypothetical numbers: the schema freeze consumes measurements that do not exist
yet, and the whole risk control of this project is that the ordering is honoured.

---

## Phase 0 — Repository skeleton & toolchain

**Goal.** A clean Linux machine goes from `git clone` to a verified toolchain
without reading a wiki page.

### Deliverables

| Path | Content |
|------|---------|
| `install.sh` | idempotent, `--dry-run`, non-root (sudo only for system packages), ends by invoking `check-env` |
| `requirements.txt` | `jinja2`, `jsonschema`, `questionary`, `pytest` — all pinned to exact versions |
| `scripts/wizard/cli.py` | argparse skeleton with every §5.1 subcommand registered; only `check-env` implemented, the rest exit 2 with "not implemented" |
| `scripts/wizard/core/` | empty packages: `model`, `validate`, `allocate`, `rte`, `generate` |
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

## Phase 1 — v0.1 reference firmware (hand-built)

**Goal.** Prove the runtime contract by hand, on one concrete configuration,
before any generator exists. Everything v1.0 later generates must first exist
here as code a person wrote and understood.

The configuration is the climate-demo of §4.4: two BME280 instances on I2C0 at
0x76/0x77, one `ClimateController` SWC instance, one PWM fan through IoHwAb,
three tasks (T10/T100/T500), no radio.

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
5. **Drv_Bme280.** Split `bme280_calc.c` (integer Bosch compensation, pure,
   host-tested) from `bme280.c` (register access). Two instances, independent
   state, `start-check-read` across three activations, 30 ms acquisition period
   on a 10 ms activation rate.
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

### Apply the two §11 patches as you write, not afterwards

1. TWDT subscription happens **after** the startup gate; the wrapper feeds **once
   per activation**.
2. SAFE_HALT **unsubscribes** from the TWDT rather than relying on blocked-task
   exemption, which is not version-robust.

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

Plus the layering negative test: CI must observe `neg_swc_includes_driver.c`
**failing** to compile. A negative test that silently stops being compiled is
worse than no test — assert on the failure explicitly.

### QEMU

Boot check only: GPIO, UART, timers. No I2C device model, so the BME280 path
runs against a fake. The test SWC emits one structured JSON line per instance;
assert on the parsed structure, never on the console transcript.

### Gate

Builds warning-free under `-Wall -Wextra`; all host tests pass under ASan and
UBSan; the negative compile test fails to compile; QEMU boots to the gate
release and emits parseable instance lines.

---

## Phase 2 — Hardware validation & measurement campaign

**Goal.** Run the §8.3 acceptance scenarios on a real HW-394 and come away with
numbers. This phase is the reason the generator does not exist yet.

### Bring-up order

Each step adds one thing that can be wrong, so a failure localises itself.

1. **PWM fan + LED only.** No bus. Proves Port init, PWM init, the task wrapper,
   the gate and the epoch.
2. **I2C probe.** Log which addresses ACK before any driver runs, so a wiring
   fault is distinguishable from a driver fault.
3. **One BME280.** Chip-ID check, one acquisition, plausible values.
4. **Both BME280s.** Independent state, distinct `sequence` counters — the
   instance model's first real test.
5. **Full loop.** Sensors → controller → fan, with supervision armed.

### Acceptance scenarios (§8.3)

Each needs recorded evidence, not a "looks right":

- two identical sensors: independent config, state, outputs, sequence counters
- sensor disconnected mid-run: NACK bounded within contract, DEGRADED after
  debounce, recovery attempted, **the other sensor unaffected**
- cached value republished: `sampleTimeUs` unchanged, age advances, consumer
  sees stale
- bus stuck low: three bounded timeouts, 9-clock recovery, cooldown, other tasks
  still meet deadlines
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

### Gate

Every scenario passes with evidence recorded in `docs/measurements.md`; every
number in the table above exists with its conditions.

### Blocked on

Open point §12.1 — the HW-394 board manifest has to be populated from a physical
board: header availability per pin, fitted pull-up values, onboard devices, and
the per-IO electrical `idleLevel` for the reset window. Do this in Phase 2, not
from a datasheet.

---

## Phase 3 — Schema freeze

**Goal.** Turn measurements into manifest data and stop moving the runtime-facing
schemas.

### Tasks

1. Write the §4 manifests for real: `interfaces/environmental.json`,
   `drivers/bme280/bme280.json`, `handcode/climatecontroller/climatecontroller.json`,
   `project.json`, `soc/esp32.json`, `modules/esp32-wroom-32.json`,
   `devkits/devkit-hw394.json`. Replace every placeholder number with a Phase 2
   measurement, with a margin policy that is written down rather than intuited.
2. Write JSON Schema (Draft 2020-12) for each manifest kind into
   `scripts/schemas/`: project, lock, interface, driver, swc, soc, module, board,
   overlay. Implement `print-schema` against these files so the schemas have
   exactly one home.
3. Verify VAL-023 closure holds with the *measured* numbers:
   `maxElapsedMs ≥ transactionTimeoutMs + recovery.budgetUs`. If it does not
   close, the contract is wrong and this is the moment to find out.
4. Ship the standard interface catalog: `EnvironmentalData`, `HealthReport`,
   `PwmDutyCycle`, `DioLevel`, `PidParams`.
5. Tag the schemas `2.1.0` frozen. After this, a change to a runtime-facing
   schema is a version bump with a migration note, not an edit.

### Gate

Every manifest validates against its schema; every executable number traces to a
Phase 2 measurement; VAL-023 closes; `print-schema` emits all nine.

---

## Phase 4 — Fixture harness (before the generator)

**Goal.** Build the thing that can tell you the output changed, *before*
anything writes output. §8.1 is explicit about this ordering and it is worth
defending: a generator built before its comparator gets its regressions
discovered by hand.

### Deliverables

| Path | Content |
|------|---------|
| `scripts/tests/fixtures/00-climate-demo/project.json` | the §4.4 composition |
| `scripts/tests/fixtures/00-climate-demo/expected/` | byte-exact expected tree — **initially a copy of `v01-reference/`** |
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

**Goal.** Reproduce fixture #0 byte-for-byte. Scope is *exactly* what the
reference path proved — nothing it did not (Appendix A, decision 15).

Build the pipeline in dependency order; each stage gets tests before the next
starts.

### 5.1 Model layer — `core/model.py`

Load and merge `project.json`, manifests, and the four hardware tiers
(SoC ∧ module ∧ board ∧ overlay). Apply the §3.2 precedence chain — explicit
project value > instance config > manifest default > interface default — and
**report every conflict it resolves**. Silent precedence is how a generator
becomes untrustworthy.

### 5.2 Validation — `core/validate.py`

All twenty-five VAL rules with their declared severities. Keep one rule per
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

Warning acknowledgment is per ID **and** per object, bound to the config-hash of
the affected subtree, so an ack stops applying when the thing it excused changes.

### 5.3 Allocation — `core/allocate.py`

A resource table, not a pin list: pins, I2C/SPI/UART controllers, LEDC timers
and channels per speed group, ADC units and channels, RMT channels.

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
soc/module/board/overlay versions, schema versions, pinned IDF and toolchain,
resolved dependencies, accepted warnings. Implement `--frozen` (reject drift),
`resolve` (refresh explicitly), and `audit` (hashes vs working tree), and wire
`audit` as a CMake pre-build target.

### 5.7 CLI and interactive steps

`new` walks S1→S9. Everything the interactive path can do, the non-interactive
path can do too — S9's review output, the VAL report, the diff against the
previous `project.json` and the lock preview are all available headless.

### Gate

`generate` on fixture #0 produces a tree byte-identical to `v01-reference/`, and
that tree still builds and still passes the Phase 1 tests.

---

## Phase 6 — v1.0 hardening & release

**Goal.** Make the guarantees checkable by CI rather than by discipline.

### Tasks

1. **Determinism job** — generate twice, byte-diff, fail on any difference.
2. **Negative compile tests in CI** — assert the layering violations *fail*.
   Add the include/symbol lint alongside, since `REQUIRES` cannot prove the
   boundary on its own.
3. **Provider-swap fixture** (REQ-ARCH-002) — replace the BME280 with a second
   type providing `EnvironmentalData` and assert the ASW output is unchanged.
   This is the single test that proves the composition model works.
4. **Interrupted-generation test** — kill mid-render, assert the previous output
   is intact and still builds.
5. **Fixture set** beyond #0: single instance; three instances; a VAL error case
   per error-severity rule; a warning-acknowledgment case; a sticky-allocation
   case.
6. **NFR check** — generation ≤ 30 s at 50 device instances / 30 SWC instances /
   8 tasks. Generate that fixture and time it.
7. **Complete `docs/traceability.md`** — every REQ mapped to design section,
   VAL/RTF and test, with no unmapped rows.
8. **Documentation pass** — README and CLAUDE.md reconciled with what actually
   shipped; every "not started" in this file resolved.

### Gate

CI green across build, host tests, generator tests, determinism, negative
compile tests and the NFR timing job. Tag v1.0.

---

## Phase 7 — v1.1

Each item is independently shippable; the ordering below is by how much it
unblocks.

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

## Phase 8 — v1.2

ESP32-S3 and C3 SoC profiles (the first real test of whether the tier model
generalises) · ULP · secure boot and flash encryption behind
`device-security --expert`, with the one-way nature stated at every prompt ·
E2E protection on Twai · Eth · Sdio · crypto with entropy gating (VAL-022:
crypto with Rng and no radio is a weak-entropy path and an Error).

## Phase 9 — v2.0

Broader driver and SWC catalog. GUI configurator over the same JSON model —
the model is the product; the GUI is a second front end onto it, and if it needs
schema changes to work, the schema was wrong.

---

## Standing rules for every phase

- **Evidence before encoding.** No measured number enters a manifest before it
  has been measured, and no phase gate passes on inspection alone.
- **Harness before generator.** Anything that emits files gets its comparator
  first.
- **The reference path is the specification.** When the generator and
  `v01-reference/` disagree, the generator is wrong until someone deliberately
  changes the reference and says so.
- **Stable identifiers.** VAL and RTF IDs are append-only. They are referenced
  from user projects, tests and the traceability matrix.
- **Declared limitations stay declared.** RTA is not a proof, the 40 MHz
  threshold is policy, QEMU cannot validate a driver, and the reset-window safe
  state is a hardware guarantee. Each phase that touches one of these restates
  it rather than quietly dropping it.
