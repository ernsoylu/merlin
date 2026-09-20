# Merlin

A project generator for ESP32 and ESP8266 ECU firmware. You describe a system
in JSON —
which sensors, on which bus, at which address, feeding which control component,
driving which actuator, on which task — and Merlin emits a complete, compilable,
deterministic project for the selected SDK with the run-time environment,
scheduler, startup sequence and supervision already wired up.

The building blocks are AUTOSAR-Classic-*style*: MCAL modules, device driver
**instances**, application SWC **instances**, connected port-to-port through
typed, versioned interfaces. **This is not an AUTOSAR implementation and claims
no conformance** — it reuses the methodology and the naming because they are a
good way to keep a firmware tree from turning into a pile of globals. It is also
not developed to ISO 26262 or IEC 61508 and carries no ASIL/SIL claim.

> **Status: current-scope runtime, schema, fixture, validation and deterministic reference-renderer work is implemented.** The
> ESP32 image builds, passes host/QEMU checks, and has booted on HW-394. The
> ESP8266/HW-364A image builds and has been visually verified on the connected
> OLED. Deterministic fake sensors cover the environmental contract. Exact
> external device drivers and their physical qualification are deliberately
> deferred; current work is internal BSW/MCAL, WLAN/BT capabilities,
> hardware-peripheral drivers and board-specific services.
> [NEXT_STEPS.md](NEXT_STEPS.md) defines the gates;
> [PROJECT_DEFINITION.md](PROJECT_DEFINITION.md) 2.2.0 is normative.

## ECU, board and driver selection

| ECU | Board profile | Default device selection | Current status |
|---|---|---|---|
| ESP32 | HW-394 | Simulated environmental provider and PWM fan | Internal runtime + boot smoke; external sensor work deferred |
| ESP8266 | Generic/raw, with actual module and wiring | No automatic OLED; choose qualified capabilities explicitly | Backend/MCAL work active |
| ESP8266 | HW-364A | Reusable SSD1306 driver instance plus reserved onboard bus pins/address | OLED build, transfer and visual baseline passed; remaining backend/radio work active |

HW-364A board selection will add the OLED automatically; its demo binds a display
SWC through RTE. The same SSD1306 driver will support explicitly wired panels on
generic ESP8266 or ESP32. BME280 is retained as a simulation/contract provider
in the portable catalog, but its exact external transport remains
non-selectable/deferred until hardware is available. ESP8266 requires a
separate native SDK/backend; the current ESP-IDF 5.2.3 setup covers ESP32 only.

The [board reference](https://github.com/peff74/esp8266_OLED_HW-364A)
configures an SSD1306 128×64 display at `0x3C`, SDA GPIO14 and SCL GPIO12.
These values need confirmation on the physical unit. Board defaults reserve
electrically occupied pins/address even if the application disables display use.
See [ECU support, compatibility and Luna handoff](docs/ecu-support.md) for the
source record, modular composition rules and local OLED acceptance tests.

Each generated project targets one ECU. Supporting more ECU types does not add
communication between boards.

## What it generates

```
project.json  ──▶  merlin  ──▶  code/          selected ECU's native SDK project
manifests     ──▶          ──▶  project.lock   what was generated, and from what
```

The generated tree contains **only** generated files. Driver and SWC sources are
referenced out of the repository via `EXTRA_COMPONENT_DIRS`, never copied — so
regenerating never overwrites hand-written code, and hand-written code never
ends up in the generated tree. Given identical inputs, generation is
byte-identical; a double-generate diff is a CI job.

## The composition model

There is exactly one wiring mechanism: ports.

| Concept | Example |
|---------|---------|
| Driver **type** | `bme280` — lives in `drivers/bme280/`, a repo component |
| Device **instance** | `ambientSensor` (bme280 @ I2C0:0x76), `enclosureSensor` (@ 0x77) |
| SWC **type** | `ClimateController` — lives in `handcode/climatecontroller/` |
| SWC **instance** | `cabinController` |
| Interface | `EnvironmentalData` — typed, versioned, **names no provider** |
| Connection | `ambientSensor.Env → cabinController.Ambient` |

A component declares the *interfaces* it needs, never the *devices*. Swapping a
BME280 for a different sensor that provides `EnvironmentalData` is a line in
`project.json`, not a change to the application. Actuators go through the same
mechanism: the generator emits an `IoHwAb` component whose instances provide
hardware interfaces (`PwmDutyCycle`, `DioLevel`), so a fan is just another port.

## Architecture of the generated firmware

This is the intended architecture; current implementation coverage is described
above. Device drivers include the available reusable SSD1306 path and a
deferred BME280 catalog entry. Each ECU has its own MCAL/runtime backend,
hardware-peripheral set and WLAN/BT capability subset.

```
┌──────────────────────────────────────────────────────────────┐
│ ASW    SWC instances — cabinController                       │
│        talk to the world only through Rte_* calls            │
├──────────────────────────────────────────────────────────────┤
│ RTE    generated per project: typed port access, coherent    │
│        sample snapshots, freshness, quality                  │
├──────────────────────────────────────────────────────────────┤
│ BSW    Os (task wrappers)  EcuM (startup/shutdown/degrade)   │
│        Hm (supervision)    Det  Log  IoHwAb                  │
├──────────────────────────────────────────────────────────────┤
│ DRV    device drivers — bme280, ssd1306 … instance-based      │
├──────────────────────────────────────────────────────────────┤
│ MCAL   Mcu Port Dio I2c Spi Uart Adc Pwm Gpt Wdg             │
└──────────────────────────────────────────────────────────────┘
```

Two access rules, and they are enforced by the build rather than by review:

- **R1** — an SWC reaches the system only through `Rte_*`. It never includes a
  driver header or an ESP-IDF header.
- **R2** — a device driver reaches hardware only through MCAL headers.

Enforcement is include/symbol linting plus **negative compile tests**: a file
that includes `bme280.h` from SWC context must *fail* to build, and CI checks
that it does. Component `REQUIRES` lists are defense-in-depth only — ESP-IDF
exposes `freertos`, the HAL and `log` to every component, so the dependency
graph alone cannot prove the boundary.

## What the runtime guarantees, and what it does not

Three things are unusual enough to state plainly.

**A sample carries its own age.** Every measurement travels as
`{values, per-element quality, sampleTimeUs, sequence}`. `sequence` advances only
on a *new* acquisition, so a driver republishing a cached reading does not
refresh its timestamp — stale data ages correctly by construction, rather than
by everyone remembering to check. Freshness is a property of the *consumer*
port (`maxAgeMs`), evaluated at read time. A default value with quality
`INITIAL` is never mistaken for a measurement.

**Supervision is generated, not bolted on.** Four levels, with their blind spots
written down:

| | Mechanism | Catches | Misses |
|---|---|---|---|
| L0 | Interrupt WDT + Task WDT | hangs | anything finer than the timeout |
| L1 | wrapper: expected vs actual wake tick | missed/late activation | a task that never resumes |
| L2 | completion vs deadline | overrun, at completion | a runnable that never returns |
| L3 | Hm: debounce, `sequence` progression, staleness | logical + wedged-driver faults | its own death |

Each level backstops the one above. Hm is a mandatory watchdog subscriber, so if
Hm itself wedges, L1 and L0 catch it. Escalation runs report → degrade → safe
state → **EcuM controlled reset** → hardware reset; a kernel panic is never a
step in that ladder.

**The safe state at reset is a hardware property.** Between power-on and
`EcuM_Startup` the pins are whatever the board's pull resistors make them. The
board manifest declares a per-IO `idleLevel` so the requirement is at least
written down, but no generated code can close the bootloader window. Failsafes
that *are* software — actuator hold-and-fail-safe on stale input — are generated
into the IoHwAb runnable and are executable, not documentation.

## Layout

```
scripts/wizard/     the generator: cli.py, steps/, core/{model,validate,allocate,rte,generate}
scripts/templates/  Jinja2, version-pinned
scripts/schemas/    JSON Schema for every manifest kind
interfaces/         the PortInterface catalog, versioned
drivers/<type>/     driver sources + <type>.json manifest + tests
handcode/<swctype>/ SWC sources + manifest + tests
soc/ modules/ devkits/ overlays/    hardware description, four tiers
v01-reference/      hand-built ESP32/host current-scope reference; becomes golden fixture #0 after qualification
v01-hw364a-reference/  hand-built ESP8266/OLED reference; future fixture #1
code/               generated output — generated artifacts only
test/               generated host-test project + QEMU model
project.json        the composition
project.lock        what was generated, from which sources, at which versions
docs/ecu-support.md modular ECU/board/driver contract and Luna handoff
docs/measurements.md observed evidence and outstanding hardware measurements
```

Hardware capability is the intersection of four tiers: **SoC ∧ module ∧ board ∧
overlay**. The SoC profile knows GPIO34 is input-only; the module profile knows
GPIO6–11 are the SPI flash and GPIO37/38 are not bonded out on WROOM-32; the
board profile knows which pins reach a header and what pull-ups are fitted.
Merlin refuses an allocation any tier forbids.

## Using it

Available now (ESP32 tooling and host tests):

```bash
./install.sh
. .venv/bin/activate
python scripts/wizard/cli.py check-env
./test/run_tests.sh
```

Current reference-renderer workflow:

```bash
./install.sh                      # pinned ESP-IDF 5.2.3 + QEMU + Python deps, idempotent
python scripts/wizard/cli.py new --non-interactive --reference climate-demo
python scripts/wizard/cli.py validate project.json
python scripts/wizard/cli.py generate
cd code && idf.py build flash monitor
```

Other commands: `configure`, `add`, `remove`, `validate`, `allocate`, `rte`, `resolve`, `audit`, `check-env`,
`add`/`remove`, `print-schema`. Exit codes: `0` ok, `1` validation error,
`2` environment error.

The guided `new` command covers the two current-scope reference compositions;
use `--non-interactive` in scripts. ESP8266 installation remains a separate
SDK environment from the ESP-IDF workflow above. Do not use the ESP32 image on
HW-364A; build its native reference with the ESP8266 RTOS SDK command in
`CLAUDE.md`.

**Allocations are sticky.** `allocate` and `add` never move an assignment that
already exists — an existing pin assignment is a constraint, not a suggestion,
because the board may already be soldered. Changing one requires explicitly
releasing that exact resource.

**The lock records; it never overrides.** `project.json` states intent,
`project.lock` records what was generated and from which source hashes, template
versions and toolchain pins. `generate --frozen` refuses to proceed on any
drift, and `audit` runs as a CMake pre-build target so the lock cannot go stale
behind your back.

## Validation

Twenty-six generation-time checks (`VAL-001`…`VAL-026`) run before a line is
emitted: resource conflicts, input-only pins, strapping pins, address
collisions, unbound ports, tick-representable timing, interface version and
structural compatibility, I2C rise time computed from the declared pull-up,
execution-contract closure, response-time analysis. Errors block generation;
warnings need an explicit per-ID, per-object acknowledgment that is recorded in
the lock.

VAL-026 covers qualified ECU/SDK/driver capabilities and board constraints;
onboard OLED pins and address also participate in ordinary allocation checks.

Seven runtime fault classes (`RTF-001`…`RTF-007`) are to be raised by generated code
and qualified by Hm with counter debounce before anything escalates.

The full table, with severities and rationale, is
[PROJECT_DEFINITION.md §5.5](PROJECT_DEFINITION.md).

## Testing

| Layer | What |
|-------|------|
| Generator | checked-in `project.json` fixtures → byte-exact golden trees; double-generate determinism |
| Host | ASW + RTE against a mocked MCAL with fault injection; `_calc.c` units against datasheet references |
| Layering | negative compile tests that CI requires to fail |
| QEMU | boot and integration only — GPIO, UART, timers. **No I2C device models**; QEMU is not a driver bench |
| Hardware | the acceptance scenarios in §8.3, which are also the measurement campaign |
| HW-364A OLED | visible patterns/counter, bounded chunks, error recovery, frame ownership and ESP8266 supervision; physical electrical limits remain deferred |

The QEMU test SWC emits one structured JSON line per instance and the test
asserts on the parsed structure — never on the console transcript, because
dual-core log ordering is not deterministic.

That integration behavior is a target requirement. The current ESP32 startup
uses the reference runtime and structured smoke records. HW-364A acceptance uses real hardware; no ESP8266 or
SSD1306 coverage is claimed from the ESP32 QEMU boot. See
[recorded evidence](docs/measurements.md).

## Roadmap

| | |
|---|---|
| **v0.1** | internal runtime, simulated environmental provider, generic ESP8266 backend and reusable SSD1306/OLED path |
| **current** | qualify internal MCAL/BSW, WLAN/BT capabilities, hardware-peripheral drivers and board-specific services on devboards |
| **freeze** | measured current-scope numbers and ECU/board/driver separation into schema 2.2.0 |
| **v1.0** | reproduce current-scope references; generic capability selection and automatic HW-364A OLED defaults |
| **v1.1** | async bus transfers, event ports, IRQ runnables, calibration over XCP-on-UART, NvM, overlays, TWAI |
| **v1.2** | ESP32-S3/C3 profiles, secure boot, Ethernet, SDIO, crypto |
| **v2.0** | GUI configurator over the same JSON model |

External device-driver qualification, beginning with BME280, follows the
current-scope generator work when the exact devices and bench hardware are
available.

The ordering is deliberate and is the main risk control in the project: the
reference path is hand-built and proven on silicon *first*, the numbers it
produces become the manifest data, and only then does the generator get written
— with those trees as fixtures #0/#1 and explicit generic ESP8266 composition tests.
