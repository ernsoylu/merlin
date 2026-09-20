# ECU targets, board profiles and driver support

Documentation contract for PROJECT_DEFINITION.md 2.2.0, updated 2026-09-20.
The ESP32 reference and host-testable SSD1306/display pieces are implemented
in `v01-reference/`; the native ESP8266/HW-364A bring-up reference is in
`v01-hw364a-reference/`. Full physical qualification remains pending.

Current hardware scope is development boards plus the connected HW-364A OLED.
Internal BSW/MCAL, target-specific services, WLAN/BT capability paths,
hardware-peripheral drivers and the SSD1306/OLED path are active. External
device drivers, beginning with BME280, remain cataloged but non-selectable and
deferred until the exact devices and bench setup are available.

## Composition and support status

Selection has three independent parts: **ECU target → board profile → device
instances**. The ECU supplies processor/SDK capabilities; the module and board
constrain wiring; a device driver supplies behavior. One project builds one ECU
image. Selecting two different ECU types does not create a network between them.

| ECU target | Board choice | Devices selected by default | Status |
|---|---|---|---|
| `esp32` | HW-394 | Climate-demo uses simulated environmental data and a fan | Internal runtime builds; QEMU and HW-394 boot smoke passed; external BME280 deferred |
| `esp8266` | Generic/raw ESP8266 | None | Baseline backend builds; concrete module/wiring and driver qualification required |
| `esp8266` | HW-364A | One `ssd1306` instance named `onboardOled`, including its I2C wiring/address reservations | Native build/boot/I2C/OLED transfer baseline passed; acceptance measurements pending |

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
| `bme280` device driver | Simulation/compensation path only; external transport deferred | Deferred; requires exact sensor and physical tests | Deferred external sensor; shared-bus qualification follows hardware availability |
| Port/Dio, I2c, Uart | DIO/UART contracts added; target evidence pending | I2C baseline implemented; UART and remaining services are not qualified | Reuses generic ESP8266 services |
| Mcu reset/reset reason | `Mcal_Mcu` maps the SDK reason to a portable code and owns `esp_restart`; builds, target evidence pending | Same adapter, same reason codes; retained boot-loop storage is still SDK-specific and unproven | Reuses generic ESP8266 adapter; software reset reason and flash path verified |
| Boot-loop supervision | `EcuM_EvaluateBootLoop` decides on retained counter, window and reset reason; `RTC_NOINIT_ATTR` storage verified on HW-394 | Same decision; retained storage is the open part | `RTC_NOINIT_ATTR` retained five injected software resets and reached SAFE_HALT; SDK has no per-task TWDT unsubscribe |
| Time/GPT/watchdog, Os/EcuM/Hm | GPT timebase and watchdog contracts added; target evidence pending | ESP8266 GPT/watchdog adapters are in the OLED path; qualification pending | Startup gate, GPT timing, watchdog feed, release/deadline path and shared Hm sequence integration build and run on the OLED task; two-cycle missed-completion debounce passed, full fault escalation remains open |
| ADC | Target capability pending | TOUT/VDD contract added; VDD depends on PHY calibration | TOUT init/read smoke passed; VDD explicitly unsupported with `vdd33_const=33` |
| Pwm/IoHwAb fan | Required by climate reference; pending | Native ESP8266 PWM adapter builds and has host contract coverage; output qualification pending | Same restriction as generic ESP8266 |
| SPI/RMT | Target-specific qualification pending | HSPI capability only; CSPI is flash-reserved and RMT is unsupported | HSPI pins conflict with the onboard OLED |
| WLAN/BT capability | WLAN declared and gated in `Mcal_Wlan_Init`; the ESP-IDF 5.2.3 `esp_netif` backend compiles, target evidence pending; Bluetooth is declared by no build | Same gate; native WLAN init/start/stop passed corrected opt-in HW-364A smoke (`0/0/0`) using strict SDK results; default disabled; Bluetooth unsupported by silicon and SDK | Same restriction as generic ESP8266 |
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

For generic ESP8266, add supported current-scope devices such as `ssd1306`
explicitly and supply their bus, pins, addresses and module/board limits.
Deferred external devices such as `bme280` remain visible for later validation
but are rejected as selectable physical targets until their hardware is present.
OLED driver defaults must not hardcode HW-364A pins. Reuse the existing SoC ∧
module ∧ board ∧ overlay model; there is no extra board-only driver layer.

Project configuration may explicitly disable use of the onboard display, but
that does not disconnect its soldered bus wiring. Keep its electrical/address
constraints until a documented physical modification or verified board variant
changes them. Reject contradictory wiring overrides rather than silently
moving the display. Changing board never relocates existing assignments without
explicit release/reconciliation (VAL-015). The board's fixed physical constraints
take precedence over configurable defaults in the §3.2 merge model.

## Backend qualification

ESP32 remains on ESP-IDF 5.2.3. The implemented ESP8266 baseline backend is
[ESP8266 RTOS SDK](https://github.com/espressif/ESP8266_RTOS_SDK), an independent
SDK. It is pinned locally to **ESP8266 RTOS SDK `v3.4`, commit
`89a3f254b63819035f65d9c5dcdae8864f1a6a8a`, with GCC 8.4.0**. The native
`v01-hw364a-reference/` build, flash hashes, boot output and repeated OLED
transfers are recorded in [measurements](measurements.md). This is a bring-up
baseline, not full qualification; the current `toolchain.env`, installer and
`check-env` still cover ESP32 only. Each backend gets isolated build outputs and
SDK environments.

The [v3.4 release](https://github.com/espressif/ESP8266_RTOS_SDK/releases/tag/v3.4),
[support policy](https://github.com/espressif/ESP8266_RTOS_SDK/blob/master/SUPPORT_POLICY_EN.md)
and [Linux setup guide](https://github.com/espressif/ESP8266_RTOS_SDK/blob/master/docs/en/get-started/linux-setup.rst)
are the starting sources. The baseline commands are
`make -C v01-hw364a-reference -j2 all`,
`make -C v01-hw364a-reference flash ESPPORT=/dev/ttyUSB0`, and the SDK monitor
at 74880 baud. Broader profile availability still requires the acceptance work.

The target schema revision must separate `target.ecu`, SoC/module/board paths,
and `target.sdk` family/revision. The lock records resolved board devices/pins,
driver sources, SDK/compiler and effective configuration. A generic ESP8266
project and an HW-364A project use the same ECU/SDK selection.

The baseline confirms one-core startup, bounded I2C command execution, the
startup display path and repeated frame completion. Full qualification still
requires effective tick/priority limits, monotonic time, critical sections,
static allocation, deadline/skip checks, watchdog task coverage, reset reasons
and retained boot-loop handling. The target uses the SDK's global
`esp_task_wdt_reset()` feed; per-task coverage and SAFE_HALT behavior remain to
be established. Do not reuse ESP32 TWDT APIs, core-affinity calls or RTC
declarations without establishing equivalents. An unsatisfied runtime
requirement leaves this target in bring-up status.

The [SDK I2C API](https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/api-reference/peripherals/i2c.html)
documents synchronous command-link operations. Inspect the pinned implementation
for actual elapsed-time bounds, allocation and error behavior. A timeout
parameter or an Arduino library call is not timing evidence. Start qualification
with Wi-Fi disabled for the OLED smoke; then implement WLAN capability
selection and backend tests as a separate active track. No Bluetooth or
ESP32-only peripheral capabilities are inherited by ESP8266; unsupported
selections must be rejected. The current reservation checker rejects HSPI's
fixed GPIO12–15 mapping when the HW-364A OLED owns GPIO12/14 and permits shared
I2C only with an explicit compatible bus key. The existing ESP32 QEMU machine does not qualify
ESP8266 or its OLED.

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

Bring-up has partially passed TST-OLED-01 and fully passed TST-OLED-02. A
2026-09-19 re-run found the OLED init NACKing on 5/5 resets as committed (root
cause: ESP8266 RTOS SDK v3.4 NACKs the first I2C transaction after
`i2c_param_config`); a fix landed in `Mcal_I2c_Init`, was re-verified across
6/6 resets and a 30 s sustained run, and the owner visually confirmed a
correct moving pattern on the physical panel. See
[measurements.md](measurements.md#2026-09-19-re-run-first-transaction-nack-found-and-fixed).

A same-day follow-up session added a real startup gate, per-activation
skip/deadline detection and TWDT feed, software fault injection (NACK burst,
forced init failure, forced software reset), and per-chunk timing/heap/stack
instrumentation to `v01-hw364a-reference/main/user_main.c`, then exercised
TST-OLED-03/04/05/07 and the skip/deadline/watchdog half of TST-OLED-08 on the
physically connected unit. A 2026-09-20 follow-up replaced the ESP8266
`RTC_DATA_ATTR` boot-loop storage with `RTC_NOINIT_ATTR`; five injected
software resets then reached `SAFE_HALT` with no sixth reset, and the clean
image was restored afterward. See
[measurements.md](measurements.md#2026-09-20-rtc-no-init-fix-and-safe_halt-re-test)
for the full record and the follow-up result. Keep hardware evidence separate from host mocks and
upstream library smoke tests in [measurements.md](measurements.md).

| Test | Required evidence |
|---|---|
| TST-OLED-01 | Partial: ESP8266EX, 2 MB, GPIO14/12 and 0x3C ACK verified; pull-up/electrical record still needs a multimeter at the bench |
| TST-OLED-02 | Passed: native Merlin init and repeated frame sequences with zero failures (re-verified 2026-09-19, 6/6 resets + 30s sustained run), and the owner visually confirmed a correct moving pattern on the physical panel the same session |
| TST-OLED-03 | Passed: per-chunk I2C write timing measured on-device, avg 2445 us / max 2529 us over 1120+ samples across a 20 s run; ~32 chunks/frame keeps full pixel transfer (~78 ms) well inside the 600 ms period and 550 ms deadline |
| TST-OLED-04 | Passed: software-injected 5-NACK burst produced exactly 1 bounded recovery (3-strikes threshold) then resumed with `failures` frozen at 5 and `completed` still incrementing on every subsequent activation -- known-position redraw confirmed. TIMEOUT and a physically stuck bus were not exercised (no bench setup for an electrical fault this session); NACK is the only fault type covered so far |
| TST-OLED-05 | Passed: init pointed at an unpopulated address on the real bus NACKed deterministically (`hw364a_oled_init result=2 native=-1`), printed `{"display":"onboardOled","health":"DEGRADED"}`, and produced zero further output over a 10 s window -- no boot loop, no false ready/frame report |
| TST-OLED-06 | Passed (host-only): existing packing/failure/recovery coverage plus a new two-instance case in `test/host/test_ssd1306.c` proving independent sequence/health/failure counters when one instance fails and the other does not |
| TST-OLED-07 | Passed: `esp_get_free_heap_size()` held constant (112620 B) across 1120+ chunk transfers and 39+ activations -- no drift, so the per-chunk `i2c_cmd_link_create`/`_delete` alloc/free pair (a real heap churn point, not "no allocation") is not leaking; task stack high-water mark 1260/2048 words. Task creation itself is a one-time heap allocation at boot (this SDK build has `configSUPPORT_STATIC_ALLOCATION` disabled), distinct from steady-state churn |
| TST-OLED-08 | Passed for the qualified reset path: the startup gate, TWDT feed-once-per-activation, skip/deadline behavior and retained five-count `SAFE_HALT` were verified with both injected software resets and tight RTS-pin resets on HW-364A (2026-09-20). The SDK has no per-task TWDT add/delete API, so SAFE_HALT parks the task |

Wrong-address fault injection can test NACK without disconnecting a soldered
panel. Electrical fault injection (TIMEOUT, stuck bus) needs a suitable bench
setup that this session did not have; record which cases were simulated and
which were physical. A visible baseline from the upstream Arduino sketches is
useful for wiring diagnosis but is not TST-OLED-02.

## Luna Code implementation sequence

1. Confirm board identity/wiring and pin the ESP8266 toolchain/backend. Record
   unresolved SDK/runtime limits before promising qualification.
2. Implement generic ESP8266 MCAL/runtime services and reusable `ssd1306`
   packing/transport, with the smallest host checks and an actual OLED demo.
   Keep HW-364A wiring in configuration. Complete the ESP32 reference in parallel.
3. Run OLED and common runtime acceptance; qualify generic ESP8266 capability
   selection with explicit wiring. Implement WLAN/backend and available
   hardware-peripheral services, rejecting unsupported capabilities. Keep BME280
   transport deferred until a real sensor is available.
4. After current-scope measurements, define the 2.2.0 target/board/driver schemas. Test HW-364A
   auto-selection, idempotence, pin/address conflicts, shared-bus compatibility,
   generic ESP8266 without an OLED, and unsupported peripheral rejection.
5. Freeze the current-scope HW-394 and HW-364A trees as fixtures #0/#1; add
   generic ESP8266 composition tests. Build the comparator before the generator,
   then implement deterministic board-default expansion and per-target
   generation. Add external-device fixtures only in the deferred phase.

The HW-364A profile is complete only when selecting it resolves the standard
SSD1306 instance and correct reservations, its generated image builds with the
pinned ESP8266 backend, and the local OLED/runtime evidence is recorded. Generic
ESP8266 support is complete only for the documented, tested driver combinations.
