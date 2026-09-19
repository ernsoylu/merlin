# ECU targets, board profiles and driver support

Documentation contract for PROJECT_DEFINITION.md 2.2.0, updated 2026-09-19.
The ESP32 reference and host-testable SSD1306/display pieces are implemented
in `v01-reference/`; target-specific ESP8266 services, manifests and physical
qualification remain pending.

## Composition and support status

Selection has three independent parts: **ECU target → board profile → device
instances**. The ECU supplies processor/SDK capabilities; the module and board
constrain wiring; a device driver supplies behavior. One project builds one ECU
image. Selecting two different ECU types does not create a network between them.

| ECU target | Board choice | Devices selected by default | Status |
|---|---|---|---|
| `esp32` | HW-394 | Climate-demo adds two external BME280s and a fan | Runtime builds; QEMU and HW-394 boot smoke passed; real sensor evidence pending |
| `esp8266` | Generic/raw ESP8266 | None | New planned target; actual module, flash and wiring must be supplied |
| `esp8266` | HW-364A | One `ssd1306` instance named `onboardOled`, including its I2C wiring/address reservations | Host driver/display slice exists; backend and physical confirmation pending |

"Raw ESP8266" is a selectable generic configuration of the same ECU backend,
not another SDK port. It still requires a concrete module/package and a wiring
description. A generic profile never asserts that every silicon pin is exposed
or free. HW-364B remains an upstream reference variant, not a qualified target.

Future ESP32-S3/C3 profiles remain in v1.2. Cross-ECU communication remains
outside v1.x scope.

## Driver compatibility

This table is the **planned support contract**, not a list of working drivers.
"Compatible" means the same driver can be selected after the corresponding
backend and device combination passes its tests. Unsupported combinations must
be rejected before generation; no empty driver stubs may count as support.

| Component | ESP32 | Generic ESP8266 | ESP8266 + HW-364A |
|---|---|---|---|
| `ssd1306` device driver | Compatible via MCAL I2c; external wiring and panel configuration required | Compatible via MCAL I2c; explicit instance/wiring | Same driver, automatically instantiated by board defaults |
| `bme280` device driver | Existing compensation helper; transport and dual-sensor runtime pending | Planned portable driver via MCAL I2c; requires physical sensor tests | Optional external sensor; share OLED bus only with verified wiring, distinct address, compatible speed and schedule |
| Port/Dio, I2c, Uart | Required MCAL services; reference implementations pending | Required target-specific MCAL services | Reuses generic ESP8266 services |
| Time/watchdog, Os/EcuM | ESP32-specific implementation of common runtime contract pending | ESP8266 adapters must be qualified | Reuses generic ESP8266 adapters |
| Pwm/IoHwAb fan | Required by climate reference; pending | Not in initial qualified subset; add only after a native implementation and tests | Same restriction as generic ESP8266 |
| Other catalog modules | Selectable only after target-specific qualification | No inheritance of ESP32 peripheral inventory | Same restriction as generic ESP8266 |

SSD1306 addressing/command generation and BME280 compensation/state machines
are device concerns. Pin assignment, GPIO timing, SDK errors and bus recovery
belong to MCAL. The shared drivers include only portable Std/Rte/MCAL headers.
Use the same sources across boards; do not create a `HW364A_Oled` driver fork.
RTE and application ports remain provider-agnostic.

## Board evidence

Inspected source: [peff74/esp8266_OLED_HW-364A at commit
15867f545c6fdb063a186e069f140c4b9e17ccc6](https://github.com/peff74/esp8266_OLED_HW-364A/tree/15867f545c6fdb063a186e069f140c4b9e17ccc6).
Its [Adafruit example](https://github.com/peff74/esp8266_OLED_HW-364A/blob/15867f545c6fdb063a186e069f140c4b9e17ccc6/HW-364A_V1.ino)
and [U8g2 example](https://github.com/peff74/esp8266_OLED_HW-364A/blob/15867f545c6fdb063a186e069f140c4b9e17ccc6/HW_364A_u8g2_1.ino)
agree on the following configuration. These are upstream observations pending
verification on the user's unit.

| Property | Reference value | Qualification needed |
|---|---|---|
| Processor / connector | ESP8266 / HW-364A USB-C | Read chip identity and inspect board/module markings |
| OLED | 0.96-inch, SSD1306 configuration, 128×64 monochrome | Verify initialization, full geometry and orientation visibly |
| I2C SDA | GPIO14 (`IO14` in Merlin) | Confirm actual wiring; upstream calls this D6 |
| I2C SCL | GPIO12 (`IO12` in Merlin) | Confirm actual wiring; upstream calls this D5 |
| Address | 7-bit `0x3C` | Probe and verify display response |
| Reset | No dedicated reset GPIO in examples (`-1` / `U8X8_PIN_NONE`) | Confirm board power/reset behavior |
| Flash | README suggests a 1 MB Arduino layout | Read actual capacity; do not treat an IDE setting as measured flash size |
| Pull-ups / voltage / exposed pins | Not established by the sketches | Inspect and record values; do not import HW-394 assumptions |

The upstream README warns that SDA/SCL may be swapped. Use numeric GPIOs as
the source values; D-label conventions vary. Resolve a wiring variant during
bring-up and record it explicitly. Firmware must not silently swap pins or
probe arbitrary alternative pin pairs in normal operation.

## Board defaults and allocation

Selecting HW-364A performs these planned composition steps:

1. Select the `esp8266` ECU and its qualified SDK profile. Merge the board's
   onboard-device defaults before dependency resolution and pin allocation.
2. Add exactly one `onboardOled` instance of the reusable `ssd1306` driver,
   configured for the board's display geometry, address and reset arrangement.
   Re-selecting/reloading the board must not duplicate the instance.
3. Select MCAL I2c through the driver's dependency. Reserve IO14 for SDA, IO12
   for SCL and address `0x3C` on that logical bus, subject to the verified board
   variant. Unrelated GPIO/PWM/SPI allocation on these pins is an error.
4. Permit another I2C device to use the **same bus instance**, pins and timing
   when compatible. Reject a second device at the same address or a competing
   bus definition. Apply co-location and total execution-budget checks.
5. Present the auto-added driver and reservations in CLI review/lock output.
   The `hw364a-oled-demo` composition binds a display SWC; board selection by
   itself adds hardware, not an arbitrary application SWC. An unbound required
   frame port is still VAL-007, with an actionable binding error.

For generic ESP8266, add `ssd1306`, `bme280` or both explicitly and supply their
bus, pins, addresses and module/board limits. OLED driver defaults must not
hardcode HW-364A pins. Reuse the existing SoC ∧ module ∧ board ∧ overlay model;
there is no extra board-only driver layer.

Project configuration may explicitly disable use of the onboard display, but
that does not disconnect its soldered bus wiring. Keep its electrical/address
constraints until a documented physical modification or verified board variant
changes them. Reject contradictory wiring overrides rather than silently
moving the display. Changing board never relocates existing assignments without
explicit release/reconciliation (VAL-015). The board's fixed physical constraints
take precedence over configurable defaults in the §3.2 merge model.

## Backend qualification

ESP32 remains on ESP-IDF 5.2.3. The proposed ESP8266 backend is
[ESP8266 RTOS SDK](https://github.com/espressif/ESP8266_RTOS_SDK), an independent
SDK. The current candidate is **ESP8266 RTOS SDK `v3.4` with its GCC 8.4.0
toolchain**: Espressif marks v3.4 as the latest release and its release notes
specify GCC 8.4.0. The SDK support policy identifies v3.4 as the LTS release
line. This is a candidate pin, not qualification evidence; Luna must verify the
tag commit, Linux install, build, flash and runtime behavior before making the
profile selectable. Record the exact commit/tool archive and dependency
environment in the lock. The current `toolchain.env`, installer and `check-env`
cover ESP32 only. Each backend gets isolated build outputs and SDK environments.

The [v3.4 release](https://github.com/espressif/ESP8266_RTOS_SDK/releases/tag/v3.4),
[support policy](https://github.com/espressif/ESP8266_RTOS_SDK/blob/master/SUPPORT_POLICY_EN.md)
and [Linux setup guide](https://github.com/espressif/ESP8266_RTOS_SDK/blob/master/docs/en/get-started/linux-setup.rst)
are the starting sources. Luna must demonstrate build/flash/monitor and document
the commands before marking it available.

The target schema revision must separate `target.ecu`, SoC/module/board paths,
and `target.sdk` family/revision. The lock records resolved board devices/pins,
driver sources, SDK/compiler and effective configuration. A generic ESP8266
project and an HW-364A project use the same ECU/SDK selection.

Qualify one-core scheduling, effective tick/priority limits, monotonic time,
critical sections, bounded I2C, static allocation, startup gate, deadline/skip
checks, watchdog task coverage, reset reasons and retained boot-loop handling.
Measure how time/reset history survives resets. Do not reuse ESP32 TWDT APIs,
core-affinity calls or RTC declarations without establishing equivalents.
An unsatisfied runtime requirement leaves this target in bring-up status.

The [SDK I2C API](https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/api-reference/peripherals/i2c.html)
documents synchronous command-link operations. Inspect the pinned implementation
for actual elapsed-time bounds, allocation and error behavior. A timeout
parameter or an Arduino library call is not timing evidence. Start qualification
with Wi-Fi disabled; no Bluetooth or ESP32-only peripheral capabilities are
inherited. The existing ESP32 QEMU machine does not qualify ESP8266 or its OLED.

## OLED runtime contract

The initial driver supports the reference's 128×64 one-bit panel configuration.
Additional geometries require explicit configuration, command support and tests.
`MonochromeFrame` is a bounded, versioned state interface: geometry, pixel
packing, frame sequence and quality are explicit. Start with 1,024 pixel bytes,
128 columns × 8 vertical pages, bit 0 the top pixel of each page. Reject wrong
dimensions or payload lengths. No heap-owned pointers cross the RTE boundary.

The display SWC provides patterns/counters through `DisplayOut`; the OLED
instance consumes `Frame` and publishes `HealthReport`. Health distinguishes
UNINIT/INITIAL, ready, communication failure and recovery. Report the last
**fully transferred** frame sequence plus transfer-failure/recovery counters;
an ACK does not prove that pixels are visible.

Use static frame storage with explicit ownership: a submitted frame remains
immutable while its chunks are transmitted. New publications may replace one
bounded pending frame; they cannot mutate the active transfer. No unbounded
frame queue. Account for every buffer in RAM budgets. Keep publication/snapshot
critical sections bounded and measured; never send I2C traffic under the RTE lock.

Budget initialization and transfer commands as well as data. Chunk frames across
activations, advance only on successful transfers, and bound retries/recovery
with cooldown. Resume or restart from a known controller address after an error.
At 100 kHz, 1,024 bytes plus ACK bits alone take 92.16 ms, before command/address
overhead; this calculation precludes copying a sensor's 2 ms timeout for a full
frame. Measure chunk WCET and full-frame latency separately and choose task
periods accordingly. Drivers sharing a bus remain co-located under VAL-019.

An absent OLED is a degradable init/communication fault; supervision and serial
diagnostics continue. Initial/invalid application data must not be displayed as
a valid live reading. A failed panel may retain old pixels, so visible content
cannot substitute for health reporting or actuator failsafes.

## HW-364A acceptance

All tests below are **planned / not run**. Keep hardware evidence separate from
host mocks and upstream library smoke tests in [measurements.md](measurements.md).

| Test | Required evidence |
|---|---|
| TST-OLED-01 | Physical identity, flash capacity, numeric wiring, pull-up/electrical record and I2C ACK on the documented bus |
| TST-OLED-02 | Merlin driver displays clear/fill, corner markers, checkerboard and changing counter correctly; visible confirmation plus serial frame sequence |
| TST-OLED-03 | Measured chunk and complete-frame timings; periodic supervision remains responsive during refresh and logging |
| TST-OLED-04 | Injected NACK/TIMEOUT/stuck-bus errors remain distinct; bounded retries/recovery/cooldown; known-position redraw after recovery |
| TST-OLED-05 | Failed init produces DEGRADED with serial diagnostics, no boot loop and no false ready/frame-complete report |
| TST-OLED-06 | Host boundary/packing checks and updates during transfer prove coherent active-frame ownership, bounded pending storage and independent instance state |
| TST-OLED-07 | Steady-state allocation trace shows no display/control-path allocate/free; static RAM and stack usage recorded |
| TST-OLED-08 | Actual startup gate, skip/deadline fault, watchdog task coverage and terminal SAFE_HALT behavior pass on the ESP8266 backend |

Wrong-address fault injection can test NACK without disconnecting a soldered
panel. Electrical fault injection needs a suitable bench setup; record which
cases were simulated and which were physical. A visible baseline from the
upstream Arduino sketches is useful for wiring diagnosis but is not TST-OLED-02.

## Luna Code implementation sequence

1. Confirm board identity/wiring and pin the ESP8266 toolchain/backend. Record
   unresolved SDK/runtime limits before promising qualification.
2. Implement generic ESP8266 MCAL/runtime services and reusable `ssd1306`
   packing/transport, with the smallest host checks and an actual OLED demo.
   Keep HW-364A wiring in configuration. Complete the ESP32 reference in parallel.
3. Run OLED and common runtime acceptance; qualify generic ESP8266 driver
   selection with explicit wiring. Port/test BME280 transport against a real
   sensor before calling that combination hardware-supported.
4. After measurements, define the 2.2.0 target/board/driver schemas. Test HW-364A
   auto-selection, idempotence, pin/address conflicts, shared-bus compatibility,
   generic ESP8266 without an OLED, and unsupported peripheral rejection.
5. Freeze the qualified HW-394 and HW-364A trees as fixtures #0/#1; add generic
   ESP8266 composition tests. Build the comparator before the generator, then
   implement deterministic board-default expansion and per-target generation.

The HW-364A profile is complete only when selecting it resolves the standard
SSD1306 instance and correct reservations, its generated image builds with the
pinned ESP8266 backend, and the local OLED/runtime evidence is recorded. Generic
ESP8266 support is complete only for the documented, tested driver combinations.
