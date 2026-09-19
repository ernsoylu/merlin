# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Merlin is a **code generator**, not firmware. It reads JSON manifests and emits
a native ESP-IDF project for ESP32-WROOM with an AUTOSAR-style layered stack
(MCAL / drivers / RTE / BSW / ASW) already wired together.

Two documents are normative and outrank anything inferred from the tree:

- **[PROJECT_DEFINITION.md](PROJECT_DEFINITION.md)** — the approved design,
  v2.1.0, FINAL. Section numbers referenced below are its sections. If code and
  that document disagree, the document wins until it is amended.
- **[NEXT_STEPS.md](NEXT_STEPS.md)** — the phased plan and where work currently
  sits. Update its status table when a phase gate passes.

**Current state: the repository contains documentation only.** No generator, no
schemas, no reference firmware. `v01-reference/` is described in §11 as
delivered *in a prior revision elsewhere* — it is not in this tree and has to be
built here (NEXT_STEPS Phase 1). Do not write code that assumes any of the
directories in §3.1 already exist.

## Commands

Nothing is implemented yet; these are the target interfaces, defined in §3.3 and
§5.1. Use them as written when you build them — scripts and CI will depend on
the spelling.

```bash
./install.sh [--dry-run]              # idempotent; pins IDF 5.2.3 + QEMU + python deps
python scripts/wizard/cli.py check-env
python scripts/wizard/cli.py validate project.json
python scripts/wizard/cli.py generate [--frozen] [--non-interactive]
python scripts/wizard/cli.py audit    # lock hashes vs working tree; also a CMake pre-build target
cd code && idf.py build               # the generated project
./test/run_tests.sh                   # host tests: plain gcc + assert, no framework, no board
pytest scripts/                       # generator tests, incl. golden-fixture comparison
```

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

**ESP-IDF is pinned at 5.2.3.** The component is `driver` — `esp_driver_i2c`
and friends exist only from 5.3 and must not appear anywhere. When checking IDF
API shapes, check them against 5.2.3, not against latest.

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

**VAL/RTF identifiers are stable.** `VAL-001`…`VAL-025`, `RTF-001`…`RTF-007` are
referenced from the traceability matrix, from `acknowledgedWarnings` entries in
user projects, and from tests. Add new IDs at the end; never renumber, never
reuse.

## Runtime rules the generated code must honour

Getting these wrong produces firmware that looks fine and is subtly untrustworthy.

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

## Testing posture

Host tests are plain `gcc` plus `assert()` with ASan and UBSan — no framework,
no fixtures. Add a unit test for any new arithmetic; it is the only evidence
that exists before a board is on the bench.

The generator's own regression is golden fixtures: a checked-in `project.json`
and the exact tree it must produce, compared byte for byte. **The fixture
harness is built before the generator** (§8.1) — that ordering is deliberate and
is Phase 4 in NEXT_STEPS. Do not start emitting files before there is something
that can tell you the output changed.
