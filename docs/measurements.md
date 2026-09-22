# Hardware evidence and measurement status

Updated 2026-09-21. This ledger distinguishes current host/QEMU/HW-394/HW-364A
bring-up evidence from the acceptance campaign still required by
PROJECT_DEFINITION.md §8.3.

### 2026-09-22 P-19 OLED visible acceptance (TST-OLED-02, partial)

The demo SWC now renders a "MERLIN" wordmark at scale 3, centred, over the
existing border and the 8-pixel sequence counter. Text was chosen over the
previous checkerboard because P-19 step 3 asks the operator to judge origin,
rotation and mirroring by eye, and asymmetric letterforms show all three where
a checkerboard shows none. The host render of the exact frame is checked in at
`docs/artifacts-p19-wordmark.txt`.

**Operator observation on the fitted panel, commit `5f6267f` plus this change,
ESP8266EX MAC `ec:64:c9:df:16:7e`:** "it shows merlin centered, no mirroring,
counter changing", and on a follow-up look, "it's upright, border visible on
all four edges". Step 3 is therefore covered in full: origin and row/column
order (text reads correctly), rotation (upright), mirroring (none) and absence
of clipping (the 1-pixel border survives on all four edges, so no row or
column is lost off-panel). The changing counter gives visible liveness. This
is the first physical confirmation that the Merlin path puts correct pixels on
the glass rather than merely completing transfers.

Firmware-side repetition for step 5, same image, six consecutive RTS-pin
resets: every cycle produced a clean `{"boot":"start","bootLoopCounter":1,
"resetReason":2}` record, restarted the frame counter at 1, advanced with no
gaps, stayed `health` READY and never reached SAFE_HALT. A sustained run of
35 s on the same image recorded 56 consecutive frames with no `rtf` records.

**Still outstanding for P-19.** Step 2's pattern
sweep (all-off, all-on, checkerboard, row and corner markers as selectable
patterns) is not implemented; only the normal moving pattern exists. Step 4's
photographs, and Section 1.5's measured current, instrument identification and
operator sign-off, are not recorded. The visual result after each of the six
resets was not observed; only the firmware record was.

**Unexplained timing change, carried forward.** `chunk_us_avg` fell from
6746 us to 2447 us (`max` 6802 -> 2525) between the checkerboard image and the
wordmark image. I2C transfer duration should not depend on frame content and
`transferFailures` stayed 0 in both, so neither figure should be treated as a
settled chunk-timing measurement for P-20 until the cause is understood. A
plausible but unverified mechanism is flash-cache/code-layout sensitivity on
this target after adding a translation unit.

### 2026-09-22 HW-364A bench run: dead deadline detection found and fixed

Re-running the HW-364A bench for the `display_task` split found a real
regression that had nothing to do with it. `Mcal_Gpt_GetTimeUs` was guarded on
`#if defined(ESP_PLATFORM) && !defined(MERLIN_HW364A)`, so on HW-364A it took
the stub branch, returned `MCAL_HW_FAIL` and never wrote `*timeUs`.
`user_main.c`'s `time_us()` discards that result, so every timestamp on that
target was 0 and every elapsed-time measurement built on it was `0 - 0`.
Deadline detection was silently dead, and chunk timing silently reported zero.
That is the "ESP32-only MCAL peripherals must never become silent stubs"
invariant being violated: `esp_timer.h`/`esp_timer_get_time()` ships in ESP8266
RTOS SDK v3.4; only `driver/gptimer.h` is genuinely ESP32-only. The guard on
the `esp_timer` half is now `#ifdef ESP_PLATFORM`; the GPTimer alarm block is
unchanged.

Evidence, commit `18198ed` plus the fix, SDK v3.4 @ `89a3f25`, GCC 8.4.0,
ESP8266EX MAC `ec:64:c9:df:16:7e`, 2 MB flash, all three flash regions
hash-verified, console 74880 Bd:

| Build | `chunk_us_avg` / `max` | RTF records |
|---|---|---|
| before fix, `HW364A_INJECT_SLOW_ACTIVATION` | 0 / 0 | `RTF-003-SKIP` only |
| after fix, same injection | 6783 / 6838 | `RTF-002-DEADLINE` **and** `RTF-003-SKIP` |

After the fix the documented TST-OLED-08 sequence reproduces exactly:
`{"rtf":"RTF-002-DEADLINE","activation":6,"elapsedUs":1517440}` for the 1300 ms
stall against the 550 ms deadline, then
`{"rtf":"RTF-003-SKIP","activation":7,"skipped":1}` as the overrun pushes
`vTaskDelayUntil` past the next boundary, with no watchdog reset. Chunk
transfers now measure ~6.75-6.85 ms each, 32 per frame (~217 ms of transfer per
activation) — a number that was previously unobtainable because the clock was
dead. This also confirms the `display_task` split did not break the deadline
path: `RTF-002` fires with `render_frame_to_panel` in place.

**Steady state on the shipped image.** A 60 s capture of the fixed
non-injection build (`5c2a92c8...`) recorded 90 frames with strictly
consecutive `completed` counters and no gaps, zero `rtf` records, zero
`SAFE_HALT`, and `health` READY throughout. `chunks_total` advanced by exactly
160 per five activations across 18 reports (32 chunks per frame), with
`chunk_us_avg` pinned at 6746 us and `chunk_us_max` settling at 6802 us.
`heap_free` held at 112256 bytes and `uxTaskGetStackHighWaterMark` at 1236
words for the whole run, consistent with no steady-state allocation churn
(a watermark alone still proves nothing about worst-case depth). Across
roughly ten resets today the boot record read `bootLoopCounter:1` every time
rather than climbing, so the `HW364A_GOOD_FRAMES_TO_CLEAR` (5) clear path is
writing RTC memory as intended.

An earlier attempt at this capture was discarded rather than interpreted: the
CH340 adapter dropped off the USB bus mid-run, producing one impossible record
(`completed` 19 -> 105965) before output stopped. It was re-run after
reconnection and is the capture described above.

**Not established by this run.** **Visible OLED pattern acceptance (P-19 /
TST-OLED-02) was not performed at all** — it needs an
operator looking at the panel, and no serial counter substitutes for it, per
the project's own rule that completed frame transfers are counted separately
from physical visible-output acceptance. Section 1.5's current measurement,
instrument calibration, photographs and operator sign-off are likewise
outstanding.

### 2026-09-21 SonarCloud static-analysis gate

The SonarCloud automatic-analysis gate failed on new code with reliability E,
security E and 67.0% duplicated lines. Reliability and security are now A:
`Nvm` covered its CRC span and its payload copies through one clamp helper
instead of indexing past fixed buffers, and the metadata renderer declares its
escaping policy explicitly. This is static-analysis evidence over the reference
sources; it is not a substitute for the host suite or for physical
qualification, and no generated binary changed behavior.

`display_task` was split: `render_frame_to_panel` now holds the submit and
bounded-chunk-transfer loop, leaving the release, deadline, watchdog and
boot-loop policy in the task. The call sits between the same `startUs` and
`elapsedUs` reads, so the timed region is unchanged, and the ESP8266 image
rebuilds clean. This edits hardware-qualified source; the bench re-run is
recorded in the 2026-09-22 entry above, which confirms the deadline path still
fires with the split in place. Visible OLED acceptance remains outstanding.

Two findings are configuration, recorded here so they are not re-litigated:

- `scripts/tests/fixtures/**/expected/**` is excluded from analysis. Every
  duplicated line was a §8.1 golden fixture mirroring `v01-reference/` byte for
  byte, which is the harness working as designed. The fixture trees are already
  asserted equal to the reference they copy, and the reference itself is
  analysed, so nothing goes unchecked. Automatic analysis ignores
  `sonar-project.properties`, so the exclusion lives in the project's
  SonarCloud analysis scope rather than in the repository.
- Both `pythonsecurity:S2083` path-traversal hits on `cli._edit_project` are
  marked false positive. The tainted value is the manifest path the operator
  typed on merlin's own command line, so there is no privilege boundary for a
  traversal to cross. `_edit_project` still rejects anything that is not an
  existing `.json` file before reading or rewriting it; confining the path to a
  base directory would break editing a `project.json` outside the working
  directory.

### 2026-09-21 Package 7N software-only closure

`check-env --target all` passed with ESP-IDF 5.2.3, Espressif QEMU 9.0.0,
ESP8266 RTOS SDK v3.4 at commit
`89a3f254b63819035f65d9c5dcdae8864f1a6a8a`, and GCC 8.4.0. The reproducible
`scripts/ci/build_esp8266.sh` wrapper built `v01-hw364a-reference` without
flashing. The shared GPTimer source now excludes ESP8266-only builds from the
ESP-IDF `driver/gptimer.h` path while retaining the ESP32 implementation.
The official `espressif/idf:release-v5.2` container built the ESP32 reference
and passed `normal`, `fake-disconnect-recovery`, `stale-failsafe` and
`deadline-skip` at three seconds each. This is software/build/QEMU evidence;
it does not close physical qualification or retained-reset behavior in QEMU.

### 2026-09-20 ESP32 QEMU current-scope smoke

The installed Espressif QEMU (`qemu-system-xtensa`, `esp32` machine) booted the
existing `v01-reference/build/qemu-flash.bin` after the image was padded to the
machine's supported 4 MiB flash size. A 2 s run produced 94 structured sensor
records; both `ambientSensor` and `enclosureSensor` stayed `READY` with strictly
increasing sequences, and `test/qemu/run_smoke.py` passed its JSON and panic
checks. This is valid ESP32 boot/integration evidence only. QEMU has no I2C
device model and provides no ESP8266/HW-364A/OLED or electrical evidence. The
required reusable flash-padding and scenario catalog now live in
`test/qemu/run_smoke.py` and `test/qemu/scenarios.json`.

### 2026-09-20 ESP32 QEMU scenario catalog

Fresh temporary ESP-IDF 5.2.3 builds passed the non-physical scenario runner:

| Scenario | Result |
|---|---|
| `normal` | 86 structured records; both sensors READY and sequences increasing |
| `fake-disconnect-recovery` | 151 structured records; transient loss/recovery assertion passed |
| `stale-failsafe` | 82 structured records; stale ambient path and failsafe assertion passed |
| `deadline-skip` | 159 structured records; RTF-002 assertion passed |

The optional `controlled-reset-safe-halt` QEMU run built successfully but did
not reproduce retained reset state, so it is not claimed as emulator evidence.
The portable EcuM state-machine test and the prior target run remain the
software/target evidence for that path. QEMU still supplies no I2C, OLED,
pull-up, PWM-load or electrical-fault model.

### 2026-09-20 HW-394 reconnect: identity and clean serial run

The newly connected USB device was identified as a CH340 adapter at
`/dev/ttyUSB0`. Read-only esptool queries identified the target as ESP32-D0WD-V3
revision 3.1, MAC `a0:dd:6c:85:88:08`, with a 40 MHz crystal, Wi-Fi/BT and 4 MB
flash at 3.3 V. No firmware was written. The board was already running the clean
reference image; a 20-second serial capture produced 1,333 structured records
(667 `ambientSensor`, 666 `enclosureSensor`), with both streams healthy and
sequences reaching 5,122. No panic, watchdog, `CONTROLLED_RESET` or `SAFE_HALT`
marker appeared. This confirms target runtime continuity only; it does not
measure pin headers, pull-ups, reset-window levels, PWM load or I2C electrical
fault recovery.

## 2026-09-19 current-scope ESP8266 adapter build

The HW-364A image was rebuilt with ESP8266 RTOS SDK `v3.4`, GCC `8.4.0` and
the prepared local Python environment. The new DIO/UART/GPT/watchdog/radio
contracts and disabled-by-default ESP8266 WLAN lifecycle hook compiled into
the image successfully. The generated `make flash`
command at 115200 baud reached the ESP8266 stub but failed twice with esptool
`Invalid head of packet (0x46)`. A direct retry at 57600 baud succeeded with
hash verification. After reset, serial output showed ESP8266 boot, 2 MB flash,
OLED initialization success, healthy completed frames 1–7, zero transfer
failures/recoveries, and stable heap/stack telemetry. The 57600-baud setting is
now the proven flash path for this CH340 connection.

The temporary `CONFIG_MERLIN_ENABLE_WLAN=y` image was then flashed at the same
baud. The first smoke exposed a contract bug: `WIFI_MODE_NULL` made the SDK
return `ESP_ERR_INVALID_ARG`, which the adapter had incorrectly treated as
success. The adapter now uses credential-free `WIFI_MODE_STA` and accepts only
`ESP_OK` from start/stop. The corrected smoke reported
`{"wlan":"lifecycle","init":0,"start":0,"stop":0}`; the OLED remained
healthy through frames 1–9 with zero transfer failures or recoveries. The
configuration was restored to WLAN disabled, rebuilt and reflashed; the normal
image then booted with no WLAN lifecycle record.

The existing `Mcal_Pwm` adapter was extended with the ESP8266 RTOS SDK native
PWM backend. Its host contract test passed, and the HW-364A image rebuilt with
the adapter compiled in. The SDK exposes a global time-based PWM group; this
adapter currently supports one channel (`timer=0`, `channel=0`) with a minimum
10 us period. It was not flashed or driven because no PWM output was connected
and the OLED pins must remain undisturbed. The rebuilt image was nevertheless
flashed at the proven 57600 baud path with all three image hashes verified;
after reset the OLED completed frames 1–7 with zero transfer failures or
recoveries, matching the prior default image.

The `Mcal_Adc` wrapper also passed its host contract test and a target smoke.
With the board's pinned PHY setting `vdd33_const=33`, VDD mode now rejects
explicitly as `MCAL_UNSUPPORTED` (`vddInit=7`); the native SDK requires `255`
for VDD measurement. TOUT mode initialized and read successfully
(`toutInit=0`, `toutRead=0`, raw sample `4`) with the input left floating. The
ADC diagnostic was temporary; the board was restored to the clean OLED image.

The ESP8266 SPI capability boundary was added and compiled into the image. It
exposes HSPI only; CSPI is reserved by flash in the pinned SDK, while HSPI uses
fixed GPIO12–15 and therefore conflicts with the HW-364A OLED's GPIO12 and
GPIO14 wiring. No SPI transfer was attempted. RMT has no supported native path
in this SDK and remains rejected.

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

### 2026-09-19 HW-394/ESP32 Phase 2 session: two real bugs, fault injection, and a deadline-miss finding

Same physically-connected HW-394 unit (ESP32-D0WD-V3 rev 3.1, MAC
`a0:dd:6c:85:88:08`, 4 MB flash), no BME280 wired this session (devkit only —
Phase 2's I2C-probe/single-sensor/dual-sensor/full-loop bring-up steps and
TST-ACC-01/04 stay blocked until a sensor is on the bench). Work therefore
focused on the scenarios that don't need a physical sensor.

**Real bug found and fixed: the boot-loop counter never persisted.**
`EcuM_Startup` declared `bootLoopCounter` as `static RTC_DATA_ATTR uint32_t`.
ESP-IDF's own header (`esp_attr.h`) documents `RTC_DATA_ATTR` as surviving
*deep sleep* only; `RTC_NOINIT_ATTR` is the one documented to survive a plain
restart. Reproduced directly: with `CONFIG_MERLIN_INJECT_SLOW_T500=5` forcing
five deadline-fault-triggered `esp_restart()` calls, the counter read back 0
on every reboot and the device cycled through 13 resets in 90 s with
`SAFE_HALT` never firing. Fixed by switching to `RTC_NOINIT_ATTR` plus an
`esp_reset_reason()`/300 s-window gate (`ESP_RST_POWERON`/`ESP_RST_BROWNOUT`
or window expiry re-arms the counter to 0), since `RTC_NOINIT_ATTR` content is
undefined on a true power-on and the design requires "5 resets *in 300 s*",
not "5 resets ever" (the prior code had no time window at all — a
long-lived device would eventually accumulate 5 lifetime resets from normal
maintenance and halt permanently). Re-verified after the fix: exactly 5
`{"system":"CONTROLLED_RESET",...}` lines, then boot 6 printed
`{"system":"SAFE_HALT","reason":"BOOT_LOOP","resetReason":3}` and stayed halted
for the remainder of a 90 s window — no 6th reset, no TWDT panic anywhere in
the log. This directly evidences **TST-ACC-10**.

**Real gap found and fixed: `resetRequested` was set but never acted on.**
`EcuM_RecordDeadlineFault` already flipped `context->resetRequested` at 5
accumulated deadline faults, but nothing in the codebase ever read it —
grepped `esp_restart` across the whole reference tree; the only hits were the
field's own declaration/assignment. Fixed in `report_fault`: on
`resetRequested`, force the fan to its failsafe duty via
`IoHwAb_FanApply(..., 0.0f, 0)` + `Mcal_Pwm_SetDuty`, log
`{"system":"CONTROLLED_RESET","reason":"DEADLINE_FAULTS","count":N}`, then
`esp_restart()` — matching "failsafe first, then reset, then RTC counter and
reason logged" exactly. This directly evidences **TST-ACC-09**, and (via the
same 5 fault-triggered resets that fed the boot-loop fix above) transitively
evidences **TST-ACC-05** (RTF-002 raised, TWDT silent throughout every cycle —
confirmed by grepping for `panic|watchdog|abort|guru` across all captured
logs: zero matches) and **TST-ACC-08** (the injected 1200 ms sleep against a
500 ms period reliably drove `vTaskDelayUntil` to return early on the next
loop, exercising `Os_ReleaseSkip`'s re-anchor path in the same cycle as the
deadline fault).

**Fault injection added:** `CONFIG_MERLIN_INJECT_SLOW_T500` (Kconfig int,
default 0), mirroring HW-364A's build-flag pattern but as a single knob whose
value *is* the injected-activation count, so N=1 exercises a lone deadline/skip
event and N≥5 additionally drives the controlled-reset/boot-loop chain with no
separate flag needed.

**Real (non-fake) I2C against an absent device — TST-ACC-06.** Flipping
`CONFIG_MERLIN_FAKE_SENSORS=n` with no BME280 wired routes `Bme280_Init`
through the genuine ESP-IDF `driver/i2c.h` path at GPIO21/22, 100 kHz, against
addresses `0x76`/`0x77` that nothing acks. This is real electrical NACK
behavior, not synthetic injection: `initFailures` climbed at a clean 200/s
(2 sensors x 100 Hz, matching the T10 period exactly), both sensors reported
`DEGRADED`, and the system stayed in `DEGRADED` for the full 20 s observation
window with zero resets and zero boot-loop activity — dependents followed
`initPolicy`, reached `DEGRADED`, no boot loop, exactly as required.

**Observability gap found and patched:** before this session, EcuM state
(`RUN`/`DEGRADED`/etc.) and sensor health were only ever printed as a side
effect of a *successful* sensor read (`report_sensor`, called only on a
sequence change). If a sensor never succeeds even once, nothing distinguishing
`DEGRADED` from a hang was ever observable on the wire. Added a low-rate
heartbeat in `run_t500` reporting state, both sensors' health, fault counters,
and the timing fields below.

**Real finding and fix: printf-driven priority inversion.** Adding the
heartbeat print (even throttled to one print per 8 T500 activations, ~4 s)
reproduced a deterministic controlled-reset/boot-loop with no fault injection:
the lower-priority T500 could hold the shared blocking stdio/UART lock while
the 9 ms-deadline T10 waited on it. The fix routes sensor, heartbeat and reset
records through the existing `Log_Ring`; a static priority-1 `LogDrain` task is
the only task-path caller of `printf`. The ring now protects concurrent ESP32
producers with one bounded critical section. The controlled-reset path yields
20 ms after enqueueing its forensic record so the drain can emit it before
`esp_restart()`. Host tests and an ESP-IDF 5.2.3 build pass. A 125 s HW-394
run after flashing this implementation produced 8,289 JSON records (4,144 per
sensor), one heartbeat with `deadlineFaults=0` and
`t10SkippedActivations=0`, and zero panic/watchdog/reset markers.
With the slow-T500 injector set to 5, the same image produced four captured
`CONTROLLED_RESET` records followed by `SAFE_HALT` on the next boot (the first
reset occurred before monitor attachment), with no panic/watchdog/abort/assert
markers; the board was then restored to the default injector value of 0.

**Clean baseline soak (throttled heartbeat, no injection).** 115 s continuous
raw-serial capture, default config (fake sensors, no injection): zero resets,
zero `SAFE_HALT`. One heartbeat snapshot at T10 wake #10,000 (100 s in):
`deadlineFaults=0`, `t10SkippedActivations=0`, `t10JitterMinTicks=0`,
`t10JitterMaxTicks=0` (N=10,000, i.e. the full 100 s at the 10 ms period —
tick-resolution, i.e. ~1 ms, not sub-tick; far short of the ≥10^6-activation
target in PROJECT_DEFINITION §8.3, which needs a multi-hour unattended run not
done this session), `publishLockMinUs=2`, `publishLockMaxUs=14` (N=6,666
samples of the RTE `Rte_EnvironmentalPublish` critical section, timed via
`esp_timer_get_time()` bracketing the call — that call is exactly the lock, no
extra code inside it). 7,666 total sensor-report lines logged over the full
115 s with no gaps, confirming T10 ran continuously the whole time.

**Fake mid-run disconnect and recovery simulation — TST-ACC-02 logic path only.**
With `CONFIG_MERLIN_FAKE_DISCONNECT_SENSOR=0`,
`CONFIG_MERLIN_FAKE_DISCONNECT_AFTER_SAMPLES=3`, and
`CONFIG_MERLIN_FAKE_DISCONNECT_FOR_ACTIVATIONS=10`, the fake transport NACKed
the ambient sensor after three completed samples, then recovered it. After
108 s the heartbeat reported `heartbeat=RUN`, both sensors `READY`,
`ambientRecoveryCount=1`, `deadlineFaults=0`, and
`t10SkippedActivations=0`; the capture contained 3,563 records per sensor
and no panic/watchdog/reset markers. This validates independent state handling
and the existing MCAL recovery callback in simulation, not electrical
unplug/recovery or real BME280 qualification.

**Fake persistent loss and stale-data failsafe simulation — TST-ACC-03 logic
path only.** With `CONFIG_MERLIN_FAKE_DISCONNECT_SENSOR=0`,
`CONFIG_MERLIN_FAKE_DISCONNECT_AFTER_SAMPLES=3`, and a zero disconnect
duration, the cached ambient sample aged out. The 108 s heartbeat reported
`ambientSensor=DEGRADED`, `enclosureSensor=READY`, `ambientSampleFresh=0`,
`fanDutyPermille=1000`, `deadlineFaults=0`, and
`t10SkippedActivations=0`, with no panic/watchdog/reset markers. This validates
the consumer freshness gate and configured fan failsafe in simulation only.

**Critical-section duration vs. target.** §6.2's target is <5 µs. Measured
min (2 µs) meets it; measured max (14-22 µs across different runs) exceeds it.
This is the first real number against that target for either ECU — worth
flagging to whoever owns the margin policy in Phase 3, not something to
silently round away.

**Structural note on steady-state heap/allocate-free (not separately
measured):** `EcuM_Startup`'s runtime state, all three task stacks, and the
FreeRTOS task control blocks are `static` (`xTaskCreateStatic`); grepping the
reference tree's runtime path (`Rte`, `Os`, `Drv_Bme280`, `ClimateController`,
`IoHwAb`, `Mcal_*`) for `malloc`/`calloc`/heap-allocating calls turns up
nothing. Zero heap churn in steady state follows structurally from there being
no allocation call in the hot path at all, which is a stronger claim than a
watermark-across-N-samples but is not the same thing as an instrumented
allocate/free trace; not attempted this session.

**Blocked pending a wired BME280 (not attempted this session, devkit only):**
TST-ACC-01 (two real sensors, independent state/sequence), TST-ACC-02 (real
mid-run disconnect vs. the "never appeared" case exercised above), TST-ACC-04
(real electrical stuck-bus / 9-clock recovery — the BME280 driver now invokes
`Mcal_I2c_Recover` after three consecutive communication failures, but the
physical path is not exercised), real per-transaction BME280 timing, and the full
sensors-to-fan control loop. HW-394's §12.1 physical board manifest (pin
availability, pull-up values, onboard devices) also still needs bench
instrumentation this session didn't have.

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

### 2026-09-20 RTC no-init fix and SAFE_HALT re-test

The connected HW-364A unit was rebuilt and flashed with the pinned SDK/GCC
path at `/dev/ttyUSB0`; all three image hashes verified. Boot-loop storage was
changed from `RTC_DATA_ATTR` to the SDK's `.rtc_noinit` section via
`RTC_NOINIT_ATTR`. A temporary `HW364A_INJECT_AUTO_RESET` build then forced two
frames followed by a software reset on each boot. The serial capture reached
`{"system":"SAFE_HALT","reason":"BOOT_LOOP","resets":5,"resetReason":2}`
after five resets and did not perform a sixth reset. This closes the
boot-loop/SAFE_HALT portion of TST-OLED-08. The temporary injection was
removed; the board was rebuilt, reflashed with the clean image, and verified
running healthy OLED frames afterward.

A separate tight RTS-pin sequence on the clean image retained the same counter
through boot values 1, 2, 3, 4 and 5, then entered the same `SAFE_HALT` decision
with `resetReason:2`; no display task frame ran in the halt boot. This confirms
the reset-pin path as well as the software-reset path.

The clean verification reported `hw364a_i2c_init result=0`,
`hw364a_oled_init result=0 native=0`, completed frames with `health:1` and
`failures:0`, and stable free heap (`112320` bytes) and stack high-water mark
(`1220` words). A 20-second clean capture measured 32-byte chunk transfers at
`3106 us` average and `3166 us` maximum over 1120 transfers. This is a repeat
measurement under the current image, not an electrical pull-up or stuck-bus
qualification.

| Qualification | State |
|---|---|
| Generic ESP8266 SDK/compiler/environment pin | Baseline verified: SDK v3.4 exact commit / GCC 8.4.0 |
| Generic ESP8266 device-driver combinations (SSD1306, BME280) | SSD1306 HW-364A baseline only; BME280 not qualified |
| HW-364A physical identity/module/flash/wiring/pull-ups/reset | ESP8266EX, 2 MB, GPIO14/12 and 0x3C verified; pull-ups/electrical record pending |
| HW-364A default OLED instance and pin/address reservation behavior | Hand-built reference verified; schema/generator tests remain later |
| TST-OLED-01…08 | TST-OLED-01 partial; TST-OLED-02 passed; TST-OLED-03/04/05/06/07 passed; TST-OLED-08 passed for startup, skip/deadline/watchdog-silence and retained five-count SAFE_HALT via software and RTS resets (2026-09-20); electrical pull-up and physical TIMEOUT/stuck-bus cases remain open |
| ESP8266 watchdog/startup/skip/deadline/SAFE_HALT qualification | Startup gate, skip re-anchor, deadline detection, TWDT-silence and five-reset SAFE_HALT verified live; this SDK still has no per-task TWDT unsubscribe API, so SAFE_HALT uses the existing parked-task behavior |
| ESP32 SSD1306 with an external panel | Host-compatible path only; not run |
| HW-394/ESP32 TST-ACC-01…10 | TST-ACC-05/08/09/10 passed (2026-09-19, via fault injection, no sensor needed); TST-ACC-06 passed (2026-09-19, real I2C against an absent device); TST-ACC-03 simulation passed (cached sample aged out and fan reached failsafe); TST-ACC-07 exercised implicitly by the RTE mechanism but not separately re-verified with dedicated instrumentation; TST-ACC-01/02/04 blocked pending a wired BME280 |
| HW-394/ESP32 boot-loop/controlled-reset qualification | `RTC_DATA_ATTR`→`RTC_NOINIT_ATTR` bug found and fixed (did not survive `esp_restart()`, mirroring the HW-364A finding below); 300 s time window added (was entirely absent — a "5 resets ever" bug); `resetRequested`→`esp_restart()` wiring gap found and fixed with failsafe-first; re-verified: 5 controlled resets → SAFE_HALT on boot 6, zero TWDT panics anywhere |

## Required measurement record

For each actual test, record ECU, board revision/module marking, device driver
and instance, wiring/address/pull-ups, supply, source revision, SDK/compiler,
effective build/flash configuration, clock and bus rates, radio state, task
schedule, instrumentation, sample count, method, observed result and artifact
paths. Distinguish host fault injection from electrical faults and record
who confirmed visible OLED behavior. Identify omitted scenarios explicitly.

| Measurement | HW-394/ESP32 | HW-364A/ESP8266 |
|---|---|---|
| Critical-section copy duration | Measured: `Rte_EnvironmentalPublish` lock 2-14 us (clean run, N=6,666) to 2-22 us (across all runs this session), N as high as several thousand (2026-09-19). Exceeds the §6.2 <5 us target at the max; min meets it | Pending, including frame publication |
| Bus transaction/state duration and fault timeout | Real hardware NACK against an absent device: ~200 initFailures/s (2 sensors x 100 Hz), no real BME280 transaction timing yet (2026-09-19) | Pixel-chunk writes measured: avg 2445 us / max 2529 us, N=1120+ (2026-09-19). Command-burst (init) and TIMEOUT/stuck-bus cases not separately measured |
| Release jitter distribution | T10: 0 ticks min/max over N=10,000 activations / 100 s clean run (2026-09-19). Tick-resolution (~1 ms), not sub-tick; far short of the ≥10⁶-activation target (needs a multi-hour unattended run) | Not measured as a distribution; one injected 1300 ms overrun exercised the skip/deadline/re-anchor path once (2026-09-19), not a jitter distribution over many samples |
| Recovery duration and cooldown behavior | BME280 driver now calls `Mcal_I2c_Recover` after 3 consecutive communication failures; physical 9-clock recovery and duration not exercised. Fake transient disconnect: one recovery, sensor resumed, cooldown held (2026-09-19) | Qualitative pass: 5 injected NACKs -> 1 bounded recovery, cooldown held, resumed correctly (2026-09-19). Recovery *duration* in us not timed |
| Execution under simultaneous bus fault and logging | Prior printf priority-inversion failure fixed by `Log_Ring` + low-priority `LogDrain`; 125 s no-injection HW-394 run passed with one heartbeat, zero deadline faults/skips, and zero panic/watchdog/reset markers (2026-09-19) | Pending, with ongoing display refresh |
| Steady-state allocate/free trace | Not an instrumented trace; structural argument only -- no malloc/calloc call exists anywhere in the runtime hot path (`Rte`/`Os`/`Drv_Bme280`/`ClimateController`/`IoHwAb`/`Mcal_*`), so heap churn in steady state is zero by construction (2026-09-19) | `esp_get_free_heap_size()` flat at 112620 B over 1120+ chunk alloc/free pairs, N=6 samples/20s (2026-09-19); no leak observed, not an allocator-level trace |
| Static RAM / task stack usage | Pending | Task stack high-water mark 1260/2048 words, stable (2026-09-19). Frame buffers (`active`/`pending`, 1024 B each) are struct fields, not separately traced |
| OLED full-frame latency / visual output | Not applicable to climate-only reference | Coarse host-side average ≈569ms/frame (2026-09-19, N=52, single run); visual pattern confirmed by owner same session; per-chunk timing now measured on-device (see above), full-frame pixel time ≈78ms derived from it |

No placeholder WCET, timeout, flash capacity or pull-up value in the design
examples is a measurement. Hardware-derived manifest values and margins must
cite a completed record here before schema/reference qualification.

### 2026-09-20 Hm integration smoke

The clean HW-364A image was rebuilt with the shared `Hm_Debounce` component
linked into the target and flashed to `/dev/ttyUSB0` (ESP8266EX MAC
`ec:64:c9:df:16:7e`, 2 MB flash, SDK v3.4/GCC 8.4.0, WLAN disabled). The OLED
startup gate released normally and a serial capture at 74880 baud reported
`health:1`, completed frames 1–11, `failures:0`, `recoveries:0`, stable free
heap of 112256 bytes and stack high-water mark of 1220 words. No false Hm
sequence fault was emitted during the healthy path. The target now records
RTF-002 deadline, RTF-003 late/skip and RTF-006 display-bus events in the Hm
runtime. A temporary compile-time `HW364A_INJECT_HM_STALL` hook then withheld
two consecutive transfer activations: serial output showed `active:1,
failed:2`, followed by `active:0, failed:0` when the queued frame completed;
OLED health remained 1 and transfer failures remained 0. The temporary hook
was removed and the clean image was restored. This qualifies sequence debounce
and healing on target; full fault escalation into EcuM/actuator policy remains
open.
