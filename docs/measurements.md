# Hardware evidence and measurement status

Updated 2026-09-19. This ledger distinguishes current host/QEMU/HW-394/HW-364A
bring-up evidence from the acceptance campaign still required by
PROJECT_DEFINITION.md §8.3.

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

## ESP8266 and HW-364A bring-up evidence

The native target is `v01-hw364a-reference/`. It reuses the shared Merlin
SSD1306 frame/DisplayDemo sources and supplies only the ESP8266 RTOS SDK MCAL
adapter. The SDK is pinned locally to v3.4 commit
`89a3f254b63819035f65d9c5dcdae8864f1a6a8a`; the Xtensa LX106 compiler reports
GCC 8.4.0. Build and flash used the 2 MB configuration and esptool reported
hash-verified writes for bootloader, application, and partition table.

The connected unit reports ESP8266EX, MAC `ec:64:c9:df:16:7e`, one core and
2 MB flash. The HW-364A wiring was exercised at SDA GPIO14 / SCL GPIO12 with a
single OLED ACK at 7-bit address `0x3C`. Native serial output showed
`hw364a_i2c_init result=0`, `hw364a_oled_init result=0 native=0`, and repeated
display records with `health:1`, increasing completed sequences, and
`failures:0`. The monitor was read at 74880 baud; direct pyserial was used when
the SDK monitor could not be attached through a non-TTY shell. The OLED is the
user-confirmed bicolor panel: physical rows 0–15 are yellow and rows 16–63 are
cyan/blue.

This establishes build, flash, boot, bus wiring, OLED ACK and repeated Merlin
driver transfers. It does not establish visible pixel correctness, timing
budgets, electrical pull-up values, sensor support, or watchdog/SAFE_HALT
qualification; those remain Phase 2 acceptance work. See
[board evidence](ecu-support.md#board-evidence).

| Qualification | State |
|---|---|
| Generic ESP8266 SDK/compiler/environment pin | Baseline verified: SDK v3.4 exact commit / GCC 8.4.0 |
| Generic ESP8266 device-driver combinations (SSD1306, BME280) | SSD1306 HW-364A baseline only; BME280 not qualified |
| HW-364A physical identity/module/flash/wiring/pull-ups/reset | ESP8266EX, 2 MB, GPIO14/12 and 0x3C verified; pull-ups/electrical record pending |
| HW-364A default OLED instance and pin/address reservation behavior | Hand-built reference verified; schema/generator tests remain later |
| TST-OLED-01…08 | TST-OLED-01 partial; TST-OLED-02 serial-transfer portion partial; TST-OLED-03…08 pending |
| ESP8266 watchdog/startup/skip/deadline/SAFE_HALT qualification | Startup and repeated transfer baseline verified; full supervision acceptance pending |
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
| OLED full-frame latency / visual output | Not applicable to climate-only reference | Pending; serial transfer works, visual confirmation not recorded |

No placeholder WCET, timeout, flash capacity or pull-up value in the design
examples is a measurement. Hardware-derived manifest values and margins must
cite a completed record here before schema/reference qualification.
