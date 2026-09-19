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

### 2026-09-19 re-run: first-transaction NACK found and fixed

Re-flashing the unmodified `4d7753d` image onto the same physically-connected
HW-364A unit reproduced `hw364a_oled_init result=2 native=-1` (NACK) on 5/5
cold and soft resets — the previously recorded "zero failures" result above
was not reproducible as committed. An I2C bus scan (address 0x01–0x77, added
temporarily and reverted) showed the panel ACKing normally at `0x3C`, ruling
out a wiring fault. Isolating further: a single throwaway zero-length
transaction issued immediately after `i2c_param_config`, and discarded,
reliably absorbs a first-transaction NACK that ESP8266 RTOS SDK v3.4's I2C
driver produces on this hardware regardless of target address; every
transaction issued after that warm-up write succeeds. The fix was applied
inside `Mcal_I2c_Init` in `v01-hw364a-reference/main/user_main.c` (the shared
MCAL entry point, not the OLED-specific caller, so a future BME280 addition on
this ECU does not re-hit the same bug) and re-verified: 6/6 consecutive soft
resets produced `hw364a_oled_init result=0 native=0`, then a 30 s sustained
run at 74880 baud recorded 52 completed frames, `health:1` throughout, and
`failures:0` — no other host tests regressed. Average full display-frame
cycle time over that run was `29.013 s / 51 intervals ≈ 569 ms` (host serial
timestamps, single run, N=52); against the fixed 500 ms post-transfer delay in
`user_main.c`, that puts full-frame transfer (init + chunked pixel writes) at
roughly 69 ms. This is a coarse, host-side average, not a per-chunk WCET —
TST-OLED-03 still needs on-device, per-chunk instrumentation before its
number is trustworthy for a timeout budget. The owner visually confirmed a
correct, moving demo pattern on the physical panel during this run, closing
the remaining visible-pattern gap in TST-OLED-02.

### 2026-09-19 follow-up: fault injection, timing, and boot-loop evidence

Same physically-connected HW-364A unit (MAC `ec:64:c9:df:16:7e`), same pinned
SDK. `v01-hw364a-reference/main/user_main.c` gained: a direct-to-task
notification startup gate (display task blocks on `ulTaskNotifyTake` until
`app_main` finishes bus/panel init and releases it); a per-activation
skip/deadline state machine ported from `Os_Wrapper.h`'s already host-tested
release-state logic (duplicated locally rather than linking the shared `Os`
component, because this SDK's build also defines `-DESP_PLATFORM`, which would
have pulled in the ESP32-only per-task TWDT block and failed to compile --
noted as follow-up cleanup, not fixed this session); `esp_task_wdt_init()`
called once after the gate opens and `esp_task_wdt_reset()` called exactly
once per activation (previously it was fed once per I2C chunk); an
`RTC_DATA_ATTR` boot-loop counter checked/incremented in `app_main`; per-chunk
I2C timing via `esp_timer_get_time()` around every `write_register` call
carrying the SSD1306 data control byte; and three independent compile-time
fault-injection paths (`HW364A_INJECT_FAILURE_BURST`,
`HW364A_INJECT_SLOW_ACTIVATION`, `HW364A_INJECT_INIT_FAILURE`,
`HW364A_INJECT_AUTO_RESET`) so faults could be produced deterministically on
real hardware without disconnecting the panel or reaching for bench
instrumentation this session didn't have. `test/host/test_ssd1306.c` gained a
two-instance case proving independent state (TST-OLED-06). Full host suite
(`./test/run_tests.sh`) passed throughout, including the new case.

**Per-chunk timing (TST-OLED-03).** Clean run (no injected faults), 20 s
capture, N=39 activations / 1120+ chunk writes: `chunk_us_avg=2445`,
`chunk_us_max=2529` (microseconds per 32-byte I2C data chunk), stable across
the whole run with no drift. 32 chunks/frame x ~2.445 ms ≈ 78 ms of I2C time
per full frame, comfortably inside the 600 ms activation period and 550 ms
software deadline used for this test. This is on-device instrumentation, not
an external bus analyzer trace.

**Fault injection, bounded recovery, known-position redraw (TST-OLED-04).**
Build with `HW364A_INJECT_FAILURE_BURST` forced exactly 5 consecutive
synthetic NACKs starting mid-run (software fault injection at the
`write_register` boundary, not an electrical fault). Observed: `failures`
climbed to 5 and then stayed at 5 for the rest of a 25 s capture; `recoveries`
reached 1 and stayed there (matches the driver's 3-consecutive-failure
threshold plus a 10-activation cooldown in `ssd1306_frame.c`, unmodified this
session); `completed` kept incrementing normally on every activation after
recovery, i.e. the display resumed transferring subsequent frames from a
correct position with no further failures. TIMEOUT and a real stuck-bus fault
were **not** exercised -- both need bench-level electrical fault injection
this session didn't have.

**Init failure -> DEGRADED, no boot loop (TST-OLED-05).** Build with
`HW364A_INJECT_INIT_FAILURE` points the OLED init at address `0x10`, which
nothing on the HW-364A bus answers. Observed on real hardware:
`hw364a_oled_init result=2 native=-1` (MCAL_NACK / ESP_FAIL) followed by
`{"display":"onboardOled","health":"DEGRADED"}`, then zero further serial
output over a 10 s window -- confirms no boot loop, no false ready/frame
report, matching the requirement exactly.

**Steady-state heap/stack (TST-OLED-07).** `esp_get_free_heap_size()` printed
every 5 activations across the 20 s clean run held constant at exactly 112620
bytes for all six samples -- no downward drift across 1120+
`i2c_cmd_link_create`/`i2c_cmd_link_delete` pairs (one alloc/free pair per
chunk; this is real per-chunk heap churn, not "no allocation" -- a stable
watermark across many iterations is evidence the pairs are balanced, not proof
by itself, consistent with the project's own caution that a watermark alone
proves nothing). `uxTaskGetStackHighWaterMark(NULL)` held constant at 1260
words free out of a 2048-word stack. Task creation (`xTaskCreate`) is a
one-time heap allocation at boot, not steady-state churn: this SDK build has
`configSUPPORT_STATIC_ALLOCATION` unset (`xTaskCreateStatic` is not even
declared), so `Os_CreateStaticTask`'s pattern from the ESP32 reference could
not be reused as-is.

**Skip/deadline/watchdog (TST-OLED-08, partial).** Build with
`HW364A_INJECT_SLOW_ACTIVATION` forces one activation to sleep 1300 ms inside
the runnable (deliberately above the 550 ms software deadline, deliberately
far below the 15 s `CONFIG_TASK_WDT_TIMEOUT_S`). Observed on real hardware, in
order: `{"rtf":"RTF-002-DEADLINE","activation":6,...}` fired for the slow
activation; the *next* activation logged
`{"rtf":"RTF-003-SKIP","activation":7,"skipped":1}` because the overrun pushed
`vTaskDelayUntil`'s target past the next boundary, exactly the re-anchor path
`Os_ReleaseSkip` implements; the board kept running normally afterward with no
watchdog reset (TWDT stayed silent throughout, confirming L1 fault reporting
is decoupled from the L2 hardware watchdog as designed). **Boot-loop/SAFE_HALT
is implemented but unverified**: ten consecutive reset trials (5 external
RTS-pin pulses via direct pyserial DTR/RTS control, mimicking esptool's own
reset sequence; 5 software `esp_restart()` calls via
`HW364A_INJECT_AUTO_RESET`, added specifically to rule out "wrong reset type"
as the explanation) all printed `{"boot":"start","bootLoopCounter":1,...}` --
the RTC-memory-backed counter never reached 2, let alone 5, so the 5-resets-
in-300s SAFE_HALT branch was never reached on real hardware. This is a real,
reproduced finding, not a test artifact: `RTC_DATA_ATTR` does not persist
resets on this ESP8266 RTOS SDK v3.4 / ESP8266EX combination the way it does
on the ESP32 reference. Root cause not yet investigated (candidates: SDK
version/config difference in how `.rtc.data` is mapped or preserved across
`esp_restart()` on this port, versus ESP32's IDF 5.2.3). Separately, this SDK
exposes no per-task TWDT add/delete API at all (only a single global
`esp_task_wdt_init()`/`esp_task_wdt_reset()`), so "unsubscribe from the TWDT
in SAFE_HALT" -- straightforward on the ESP32 reference via
`esp_task_wdt_delete(NULL)` -- has no direct equivalent here and needs its own
design decision before TST-OLED-08 can be called passed.

**Not attempted this session:** TST-OLED-01's pull-up/electrical measurement
(needs a multimeter at the bench); TIMEOUT and stuck-bus fault injection
(needs bench-level electrical fault capability); HW-394/ESP32 §8.3 scenarios
and the HW-394 board-manifest population from §12.1 (a different, physically
disconnected board this session). The board was left flashed with the clean,
non-injection build and re-verified running normally (`health:1`, `failures:0`)
before ending the session.

| Qualification | State |
|---|---|
| Generic ESP8266 SDK/compiler/environment pin | Baseline verified: SDK v3.4 exact commit / GCC 8.4.0 |
| Generic ESP8266 device-driver combinations (SSD1306, BME280) | SSD1306 HW-364A baseline only; BME280 not qualified |
| HW-364A physical identity/module/flash/wiring/pull-ups/reset | ESP8266EX, 2 MB, GPIO14/12 and 0x3C verified; pull-ups/electrical record pending |
| HW-364A default OLED instance and pin/address reservation behavior | Hand-built reference verified; schema/generator tests remain later |
| TST-OLED-01…08 | TST-OLED-01 partial; TST-OLED-02 passed; TST-OLED-03/04/05/06/07 passed (2026-09-19 follow-up); TST-OLED-08 partial (skip/deadline/watchdog-silence verified, boot-loop/SAFE_HALT unverified -- RTC persistence gap found) |
| ESP8266 watchdog/startup/skip/deadline/SAFE_HALT qualification | Startup gate, skip re-anchor, deadline detection and TWDT-silence verified live (2026-09-19); SAFE_HALT boot-loop path implemented but not reachable on real hardware pending the RTC persistence fix; no per-task TWDT unsubscribe API on this SDK |
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
| Bus transaction/state duration and fault timeout | Pending | Pixel-chunk writes measured: avg 2445 us / max 2529 us, N=1120+ (2026-09-19). Command-burst (init) and TIMEOUT/stuck-bus cases not separately measured |
| Release jitter distribution | Pending, T10 ≥10⁶ activations | Not measured as a distribution; one injected 1300 ms overrun exercised the skip/deadline/re-anchor path once (2026-09-19), not a jitter distribution over many samples |
| Recovery duration and cooldown behavior | Pending | Qualitative pass: 5 injected NACKs -> 1 bounded recovery, cooldown held, resumed correctly (2026-09-19). Recovery *duration* in us not timed |
| Execution under simultaneous bus fault and logging | Pending | Pending, with ongoing display refresh |
| Steady-state allocate/free trace | Pending | `esp_get_free_heap_size()` flat at 112620 B over 1120+ chunk alloc/free pairs, N=6 samples/20s (2026-09-19); no leak observed, not an allocator-level trace |
| Static RAM / task stack usage | Pending | Task stack high-water mark 1260/2048 words, stable (2026-09-19). Frame buffers (`active`/`pending`, 1024 B each) are struct fields, not separately traced |
| OLED full-frame latency / visual output | Not applicable to climate-only reference | Coarse host-side average ≈569ms/frame (2026-09-19, N=52, single run); visual pattern confirmed by owner same session; per-chunk timing now measured on-device (see above), full-frame pixel time ≈78ms derived from it |

No placeholder WCET, timeout, flash capacity or pull-up value in the design
examples is a measurement. Hardware-derived manifest values and margins must
cite a completed record here before schema/reference qualification.
