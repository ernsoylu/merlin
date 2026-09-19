# Hardware evidence and measurement status

Updated 2026-09-19. This ledger distinguishes current host/QEMU/HW-394 smoke
evidence from the acceptance campaign still required by PROJECT_DEFINITION.md
§8.3.

## Existing ESP32 smoke evidence

Results below come from the preceding development-session tool output. Full
serial/build logs were not archived in the repository; repeat and retain raw
artifacts for formal acceptance. Source baseline: `2cc84f6` before these docs.

| Observation | Result | What it establishes |
|---|---|---|
| `./test/run_tests.sh` | Thirteen host units passed ASan/UBSan; BME280 and OLED forbidden includes were rejected | Host/runtime and layering checks pass; no physical timing evidence |
| ESP-IDF build | Completed with SDK 5.2.3; application binary `0x30580` bytes; 4 MB image header | ESP32 reference is buildable |
| ESP32 QEMU | Parsed structured records for both sensors, `READY`, increasing sequences, no panic | Startup/tasks/integration smoke; QEMU has no I2C device model |
| Connected USB device | CH340 serial adapter at `/dev/ttyUSB0` | Serial connection, not proof of board SKU |
| Connected chip | ESP32-D0WD-V3 revision 3.1, 40 MHz crystal | Chip identity reported by flashing tool |
| Flash and boot | Bootloader/partition/app writes hash-verified; app returned normally | Current reference image ran on connected HW-394 |
| Flash capacity | Device reports 4 MB and image header now reports 4 MB | Flash configuration matches this unit |
| Application output | `ambientSensor` and `enclosureSensor`, `READY`, sequences 1+ at 2508 centi-degrees C | Runtime/task path works with deterministic fake sensors |
| Monitor output | ESP-IDF 5.2.3, 4 MB flash, repeated structured records, no panic/abort/SAFE_HALT | HW-394 boot smoke passed; external sensor/fan wiring is not proven |

The connected chip/adapter establishes the serially identified ESP32-D0WD-V3
and 4 MB flash for this smoke run, but does not establish HW-394 pinout,
pull-ups, fan wiring, or external BME280 presence. The default image uses fake
sensor transport; real sensor readings and Phase 2 measurements remain open.

The layering suite has a valid positive SWC compile control and diagnostic-
specific forbidden-include checks for both BME280 and SSD1306. The display
SWC/driver path is host-tested with injected transfer success/failure; no OLED
panel is connected in this record.

## New ESP8266 and HW-364A scope

No ESP8266 device has been identified or flashed in this recorded session. The
Merlin SSD1306 implementation and display SWC now have host evidence, but no
OLED hardware result exists. The upstream repository supplies reference
configuration only; see
[board evidence](ecu-support.md#board-evidence).

| Qualification | State |
|---|---|
| Generic ESP8266 SDK/compiler/environment pin | Candidate: ESP8266 RTOS SDK v3.4 / GCC 8.4.0; exact artifacts and local verification pending |
| Generic ESP8266 device-driver combinations (SSD1306, BME280) | Not qualified; no measured support claim |
| HW-364A physical identity/module/flash/wiring/pull-ups/reset | Pending |
| HW-364A default OLED instance and pin/address reservation behavior | Documentation only; future schema/generator tests |
| TST-OLED-01…08 | All not run |
| ESP8266 watchdog/startup/skip/deadline/SAFE_HALT qualification | Pending |
| ESP32 SSD1306 with an external panel | Host-compatible path only; not run |

## Required measurement record

For each actual test, record ECU, board revision/module marking, device driver
and instance, wiring/address/pull-ups, supply, source revision, SDK/compiler,
effective build/flash configuration, clock and bus rates, radio state, task
schedule, instrumentation, sample count, method, observed result and artifact
paths. Distinguish host fault injection from electrical faults and record
who confirmed visible OLED behavior. Identify omitted scenarios explicitly.

| Measurement | HW-394/ESP32 | HW-364A/ESP8266 |
|---|---|---|
| Critical-section copy duration | Pending | Pending, including frame publication |
| Bus transaction/state duration and fault timeout | Pending | Pending, commands and display chunks separately |
| Release jitter distribution | Pending, T10 ≥10⁶ activations | Pending at selected/recorded task periods |
| Recovery duration and cooldown behavior | Pending | Pending |
| Execution under simultaneous bus fault and logging | Pending | Pending, with ongoing display refresh |
| Steady-state allocate/free trace | Pending | Pending |
| Static RAM / task stack usage | Pending | Pending, account for every frame buffer |
| OLED full-frame latency / visual output | Not applicable to climate-only reference | Pending |

No placeholder WCET, timeout, flash capacity or pull-up value in the design
examples is a measurement. Hardware-derived manifest values and margins must
cite a completed record here before schema/reference qualification.
