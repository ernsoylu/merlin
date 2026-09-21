# Merlin physical qualification procedure

**Document status:** procedure, not a result record
**Scope:** HW-394/ESP32, HW-364A/ESP8266, current internal peripherals,
v1.1 physical extensions, and the deferred external-device path
**Authority:** this procedure complements [NEXT_STEPS.md](../NEXT_STEPS.md),
[PROJECT_DEFINITION.md](../PROJECT_DEFINITION.md),
[docs/ecu-support.md](ecu-support.md), and
[docs/measurements.md](measurements.md)

This document defines how physical qualification shall be performed. It does
not turn host tests, QEMU, a serial log, or a simulated fault into physical
evidence. Every test shall record the exact board, wiring, instrument, source
revision, firmware image, SDK/toolchain, configuration, measured values and
operator. A test is **passed** only when its required physical observation and
its recorded numerical evidence both exist.

The operator shall not change a board manifest, timeout, pull-up value, task
period, recovery budget or qualification status during a test run. If a value
must change, the run shall stop, the change shall be recorded, and the test
shall restart from preparation.

## 1. Qualification rules

### 1.1 Evidence levels

| Evidence | Meaning | Qualification use |
|---|---|---|
| H0 | Host or static analysis | Software contract only; never physical qualification |
| Q0 | QEMU or mocked peripheral | Portable runtime/fault policy only; never board/electrical evidence |
| T0 | Target build/flash/serial boot | Target bring-up only |
| T1 | Target measurement with an instrument or identified physical device | Physical evidence for the measured condition |
| T2 | Repeated target measurement under declared limits and fault conditions | Qualification evidence |

T1 is not automatically T2. Repetition, sample count, environmental
conditions and acceptance margin shall be stated for every T2 claim.

### 1.2 Common safety rules

The operator shall:

1. Use a current-limited 3.3 V supply or a known-good USB supply appropriate
   for the board. The operator shall not inject 5 V into a 3.3 V GPIO, I2C,
   SPI, UART, ADC or reset line.
2. Disconnect power before changing jumpers, pull-ups, sensor wiring, bus
   fixtures or transceiver wiring.
3. Never force an I2C line high with a push-pull source. A stuck-low fixture
   shall use an open-drain device, a transistor/MOSFET fixture, or a passive
   jumper to ground through the approved protection arrangement.
4. Stop immediately for unexpected temperature rise, smoke, smell, excessive
   current, repeated brownout, uncontrolled actuator movement, exposed mains
   voltage, or a logic level outside the board's voltage range.
5. Attach oscilloscope ground only to the approved board ground. Avoid a second
   ground path through an earthed instrument when the supply arrangement is
   not known.
6. Restore clean firmware and normal wiring after every injected-fault test.

### 1.3 Required common equipment

The qualification bench shall contain, as applicable:

- HW-394 ESP32 and HW-364A ESP8266 devkits;
- the exact SSD1306 panel fitted to HW-364A and its documented cable;
- the exact BME280 module(s) when Phase 8 starts;
- regulated 3.3 V supply with current limit, USB data cables and a second
  USB-UART adapter where independent logging is useful;
- DMM with resistance, continuity, voltage and current measurement;
- oscilloscope with at least two channels and suitable 3.3 V probes;
- logic analyzer capable of I2C, SPI, UART and, if used, CAN/TWAI decode;
- jumper wires, header probes, open-drain fault fixture and resistors;
- calibrated pulse/square-wave generator or a clean timer-output source;
- Wi-Fi access point with a fixed test SSID and credentials;
- CAN/TWAI transceiver, 120 ohm termination and a second CAN node/analyzer;
- a fan or electronic PWM load and tachometer where applicable;
- thermal probe or calibrated temperature reference;
- camera or written visual checklist for OLED evidence;
- computer with pinned toolchains, serial capture and instrument software.

The operator shall list the actual equipment model and calibration date in the
test record. “Oscilloscope used” without model, bandwidth/sample rate and
probe setting is incomplete evidence.

### 1.4 Common firmware preparation

Before each campaign, the operator shall:

1. Record Git commit, dirty/clean state, project lock, SDK revision, compiler,
   board name, module marking and flash size.
2. Build the target with the intended `sdkconfig` in a dedicated build
   directory. Save the application, bootloader and partition-table hashes.
3. Confirm serial capture at the documented baud and record the timestamp
   source.
4. Disable unrelated radios and loads unless the test enables them. Record
   CPU frequency, task periods, priorities, core affinity, bus rates,
   pull-ups and radio state.
5. Flash the clean image, verify the flash hash, reset the board and capture a
   clean startup before attaching a fault fixture.
6. Use measurement instrumentation only when its changes are recorded and do
   not add blocking or slow logging to the measured path.

### 1.5 Required test record

Every test record shall contain:

```text
Test ID and procedure revision:
Date/time and operator:
Board / module marking / MAC / flash size:
ECU and SDK/compiler revision:
Git commit and firmware artifact hashes:
Power source, voltage, current limit and measured current:
Wiring, pin map, bus address, cable and pull-up values:
Instrument models, settings and calibration dates:
CPU clock, bus clock, radio state, task configuration:
Fixture description and fault-injection method:
Sample count, duration, repetitions and environmental conditions:
Observed values and artifact paths:
Expected result:
Pass / fail / blocked / not applicable:
Anomalies, deviations and follow-up action:
Operator sign-off:
```

Raw serial logs, scope/logic-analyzer traces, decoded captures, photographs
and scripts shall be retained. A summary number without raw evidence is not
sufficient for a manifest value.

The common firmware preparation and safety rules apply to every test below.
Where a test does not repeat a separate **Preparation** heading, the operator
shall still complete Sections 1.2 and 1.4 and shall perform the test-specific
fixture and wiring checks stated under **Equipment** and **Steps**.

## 2. Board identity and electrical baseline

### P-01 — HW-394 identity, header and board-manifest inventory

**Purpose:** establish the physical facts that the HW-394 board manifest may
claim.

**Equipment:** HW-394, DMM, camera, continuity probes, USB power meter and
oscilloscope.

**Preparation:** power off. Photograph both sides and all headers. Record
module, regulator, crystal, flash, USB-UART bridge, LED, reset/boot buttons
and connector labels. Load the board pin table without assuming a sales-board
label is a GPIO number.

**Steps:**

1. Identify the ESP32 module and exact marking/revision.
2. Confirm flash size through the toolchain and compare it with the physical
   module record.
3. Use continuity from every declared header pin to the module/onboard device;
   record pins that are not exposed.
4. Identify flash-connected pins, USB-UART pins, buttons, LED and soldered
   pull-up/pull-down networks.
5. With power removed, measure resistance from every declared I2C line to
   3.3 V and ground. Document parallel in-circuit contributors.
6. Measure 3.3 V and current at idle and during maximum internal-runtime
   load.
7. Probe each reset-window IO during power-on, reset assertion and release.

**Expected outcome:** physical pin/header/device/pull-up/idle-level data match
the manifest, or every mismatch is recorded as a qualification block.

**Pass criteria:** all selectable current-scope resources are identified and
every electrical manifest value has a measured source.

### P-02 — HW-364A identity, OLED wiring and pull-up inventory (TST-OLED-01)

**Purpose:** close the electrical and identity portion of TST-OLED-01.

**Equipment:** HW-364A, fitted OLED, DMM, continuity tester, oscilloscope,
camera, USB-UART and current meter.

**Preparation:** power off and remove the OLED cable for continuity tests.
Record ESP8266/module marking, flash configuration, MAC, board revision,
controller marking and connector labels.

**Steps:**

1. Verify SDA/SCL continuity and record GPIO numbers, not only D-labels.
2. Scan the bus and verify the expected 7-bit address `0x3C`.
3. Measure pull-up resistance with power removed and identify parallel paths.
4. Measure SDA/SCL idle-high voltage, low level, rise time and fall time at
   the OLED connector at the selected bus rate.
5. Confirm reset, boot-strap and UART pins are not accidentally used by OLED
   wiring.
6. Record panel geometry, controller marking, row color arrangement and
   cable orientation with photographs.

**Expected outcome:** GPIO14/GPIO12, address `0x3C`, module/flash identity and
the actual pull-up/electrical record are reproducible and match the profile.

**Pass criteria:** no board-specific value remains only a sales-listing
assumption; measured pull-up and rise-time evidence exists.

### P-03 — Power, reset and boot-window safe-state test

**Targets:** HW-394 and HW-364A separately.
**Equipment:** current-limited supply, DMM, oscilloscope with four channels,
reset/EN probe, optional logic analyzer and safe actuator/load.

**Steps:**

1. Probe reset/EN, each actuator output, I2C lines and declared board LED.
2. Power-cycle at the minimum and nominal permitted supply voltage.
3. Capture power assertion, bootloader, reset release and application startup.
4. Assert reset with the board control and repeat the capture.
5. Repeat with external display/actuator cables connected and disconnected.
6. Record every output level before application initialization. Repeat at least
   10 cold starts and 10 reset-button/EN starts.

**Expected outcome:** every reset-window output remains at its declared safe
idle level; no fan or actuator energizes unintentionally; gated application
tasks do not run before startup release.

**Pass criteria:** no unsafe pulse, unexpected bus drive or uncontrolled
actuator operation occurs in any repetition.

### P-04 — Supply margin, current and thermal baseline

**Targets:** each board and each qualified load configuration.
**Equipment:** regulated supply or USB power meter, DMM, oscilloscope,
thermal probe or IR camera, OLED, fan/load and WLAN AP where applicable.

**Steps:**

1. Measure current and 3.3 V at clean idle.
2. Measure current during maximum runtime load, OLED transfer, PWM load,
   WLAN association/traffic and NvM write where applicable.
3. Capture supply ripple and minimum voltage at the board pins.
4. Run the declared sustained duration and record module, regulator,
   USB-UART and load temperature.
5. Repeat after a cold start and after thermal stabilization.

**Expected outcome:** voltage remains within board/device limits, no brownout
or reset occurs, and thermal behavior is stable for the declared duration.

**Pass criteria:** voltage, ripple, current and temperature have documented
margins; USB-only behavior is not used as board power qualification.

## 3. Common runtime and peripheral qualification

### P-05 — Clean target boot, flash integrity and repeated reset

**Targets:** HW-394 and HW-364A.
**Equipment:** target, USB-UART, flash tool, power/reset controls, serial
capture and optional current meter.

**Steps:**

1. Prepare the declared clean flash state.
2. Flash bootloader, partition table and application; record hashes.
3. Capture startup, target identity, health records and first supervised
   activation.
4. Perform at least 20 cold power cycles and 20 software/reset-pin cycles.
5. Count panics, brownouts, boot-loop increments, init failures, watchdog
   events and unexplained resets.

**Expected outcome:** every clean boot reaches the declared healthy state,
reports the expected identity and produces no unexplained fault.

**Pass criteria:** 100% repeatable boot for the declared sample count and a
hash-verified image.

### P-06 — GPIO, UART and DIO physical function

**Targets:** selectable GPIO/UART/DIO paths on each board.
**Equipment:** logic analyzer or oscilloscope, safe loopback wire, USB-UART,
DMM and known digital load.

**Steps:**

1. Drive each selected DIO output low/high and measure voltage and transition
   time at the header and load.
2. Read back input state using a safe jumper or signal source.
3. Exercise UART transmit/receive and framing at the declared baud; decode it
   with a second receiver or loopback.
4. Repeat with supervision and logging active.
5. Open/close the load where the contract permits and verify independent fault
   reporting.

**Expected outcome:** logic levels, baud, framing and input state match the
contract; reserved pins are not driven; unrelated tasks meet deadlines.

**Pass criteria:** measured levels/timing are inside limits and selected
GPIO/UART combinations behave independently.

### P-07 — PWM/GPT output and failsafe load behavior

**Targets:** HW-394 PWM and HW-364A PWM when a safe physical output exists.
**Equipment:** oscilloscope/logic analyzer, DMM, safe fan/electronic load,
tachometer if applicable and current meter.

**Steps:**

1. Capture frequency, period, duty, rise/fall time and idle level at 0%, 50%
   and 100% duty.
2. Repeat at the minimum and maximum supported configuration.
3. Inject stale-input or supervision fault and verify the physical failsafe.
4. Run the maximum declared load and measure current, temperature and waveform.

**Expected outcome:** duty/frequency are within tolerance, startup/reset are
safe, and stale/fault input reaches the documented failsafe.

**Pass criteria:** waveform, load response and failsafe transition are
measured; serial output alone is insufficient.

### P-08 — ADC physical input and unsupported-channel confirmation

**Targets:** selectable ADC channels; HW-364A TOUT where used.
**Equipment:** calibrated voltage source or resistor divider, DMM and
oscilloscope for dynamic input.

**Steps:**

1. Confirm input pin and voltage range from the measured board inventory.
2. Apply at least five known voltages across the declared range.
3. Record raw counts, converted value, noise, repeatability and saturation.
4. Repeat at cold boot, warm runtime and maximum unrelated load.
5. Exercise unsupported channel selection only through validation/diagnostics.

**Expected outcome:** supported ADC is monotonic and bounded; unsupported
selection is rejected without corrupting another service.

**Pass criteria:** calibration/error margin and unsupported result are recorded
per ECU; measurements are not copied between ESP32 and ESP8266.

### P-09 — SPI capability and resource-conflict check

**Targets:** only physically selectable SPI instances.
**Equipment:** SPI logic analyzer, known-good SPI peripheral or loopback,
continuity tester and OLED fixture where applicable.

**Steps:**

1. Confirm SCK/MOSI/MISO/CS continuity and idle levels.
2. Transmit a known pattern at each declared clock rate and decode it.
3. Exercise chip-select ownership for each supported sharing mode.
4. On HW-364A, attempt HSPI selection with the OLED reservation present.
5. If a verified wiring variant exists, record the modification and repeat
   P-02 before testing it.

**Expected outcome:** supported transfers decode correctly; OLED/HSPI overlap
is rejected unless a compatible physical variant is verified.

**Pass criteria:** no automatic pin remapping or silent reservation override.
Unsupported SPI is a valid recorded result.

### P-10 — Radio startup, operation, shutdown and isolation

**Targets:** ESP32 WLAN; ESP8266 WLAN only when explicitly selected and
backend-supported. Bluetooth is not selectable for the current boards unless a
future board/SDK proves it.

**Equipment:** fixed Wi-Fi AP, second network client, serial capture, current
meter and optional timing probes.

**Steps:**

1. Record clean radio-disabled current, startup and task timing.
2. Enable only supported WLAN; verify initialization, association, packet
   exchange and clean stop.
3. Record association time, steady current, disconnect/reconnect, resource
   behavior and task deadlines.
4. Repeat shutdown/restart at least 10 times.
5. Attempt unsupported Bluetooth or incompatible radio only through validation
   and record rejection.
6. Verify radio activation does not consume an ADC2, core, pin or bus resource
   claimed by another service.

**Expected outcome:** supported WLAN starts, exchanges data, stops and restarts
without contaminating display/control; unsupported radio is rejected before
target operation.

**Pass criteria:** capability, reservation, startup/shutdown, current and
fault behavior are recorded per ECU.

### P-11 — Startup gate, skipped activation and wake-jitter measurement (TST-ACC-08)

**Targets:** HW-394 and HW-364A.
**Equipment:** oscilloscope/logic analyzer, optional GPIO markers and serial
capture.

**Preparation:** add documented nonblocking markers at release and activation
boundaries.

**Steps:**

1. Capture reset release, initialization completion and first activation.
2. Confirm no gated task runs before release.
3. Capture at least 10,000 activations initially and target at least 1,000,000
   T10 activations for final evidence.
4. Compute period error, wake jitter, skips and deadline misses.
5. Repeat with logging, maximum internal load, radio and display/bus active.

**Expected outcome:** the startup gate releases each task once; timing remains
within budget; misses are counted without back-to-back release storms.

**Pass criteria:** raw edge trace and distribution exist for each ECU. The
measured distribution, not another board's value, feeds the manifest.

### P-12 — Sample freshness, coherence and cross-core transfer (TST-ACC-03, TST-ACC-07)

**Targets:** HW-394 RTE path; cross-core path only when selected.
**Equipment:** oscilloscope/logic analyzer with markers, maximum-load image
and serial capture.

**Steps:**

1. Mark entry/exit of the target RTE copy or critical section.
2. Measure duration for normal/max payload and concurrent producer/consumer.
3. Change every value and metadata field together at independent producer and
   consumer rates.
4. For cross-core operation, record read retries/failures and any torn value
   or metadata combination.
5. Repeat with logging, radio and peripheral traffic active.
6. Republish a cached sample without a new acquisition. Verify
   `sampleTimeUs` and `sequence` remain unchanged while age advances.
7. Continue until the consumer declares the sample stale and verify the
   physical actuator/display policy reaches its documented safe behavior.

**Expected outcome:** value, quality, `sampleTimeUs` and `sequence` are always
coherent; bounded readers fail rather than spin forever; measured duration is
inside the declared budget.

**Pass criteria:** zero torn snapshots and a complete duration distribution.

### P-13 — Supervision, overrun and watchdog independence (TST-ACC-05)

**Targets:** HW-394 and HW-364A.
**Equipment:** target, serial capture, reset/EN/RTS controls, optional GPIO
marker and current meter.

**Steps:**

1. Run a clean workload and record activation, completion, deadline, watchdog
   and health counters.
2. Inject one controlled overrun below the TWDT timeout. Verify RTF-002 or
   equivalent deadline evidence while TWDT remains silent.
3. Repeat overruns until the declared escalation applies.
4. Verify failsafe action precedes controlled reset.
5. Repeat under simultaneous bus fault and logging load.
6. Verify unrelated tasks continue within their deadlines during degradation.

**Expected outcome:** deadline detection is independent of the watchdog,
escalation is deterministic, logs remain bounded and failsafe precedes reset.

**Pass criteria:** timestamped serial and instrument evidence show
`fault -> failsafe/degrade -> controlled reset`, without unexplained panic.

### P-14 — Controlled reset forensics and five-reset SAFE_HALT (TST-ACC-09, TST-ACC-10)

**Targets:** HW-394 and HW-364A.
**Equipment:** reset control or RTS/EN fixture, power switch, serial capture,
optional current meter and GPIO probes.

**Steps:**

1. Start from clean retained memory and record reset reason.
2. Trigger five qualified controlled resets within 300 seconds, using software
   and physical reset paths where applicable.
3. Capture retained count, reason, failsafe output and task activity at each
   boot.
4. Observe the sixth boot and verify terminal SAFE_HALT, no sixth reset and no
   supervised display/control task activity.
5. Verify counter clear after sustained RUN and reinitialization on power-on/
   brownout according to the target contract.

**Expected outcome:** the retained counter survives the intended reset path;
five resets lead to stable SAFE_HALT; safe outputs apply before halt.

**Pass criteria:** boot-by-boot count/reason/output records exist, and software
and physical reset results are distinguished.

## 4. HW-394 climate and external-sensor campaign

These tests require exact BME280 hardware and wiring. Until it is available,
the simulation provider may exercise software policy, but these tests remain
blocked and shall not be marked physically passed.

### P-15 — Two real BME280 sensors and independent state (TST-ACC-01, TST-ACC-06)

**Equipment:** two identified BME280 modules, HW-394, I2C wiring, measured
pull-ups, DMM, logic analyzer and calibrated environmental reference if
available.

**Preparation:** verify supply, SDO/address strap, decoupling and bus wiring.
Configure distinct addresses, normally `0x76` and `0x77`. Record markings.

**Steps:**

1. Scan the bus and confirm both addresses.
2. Start both instances and capture independent initialization, sequence,
   sample time and quality records.
3. Thermally disturb one sensor while leaving the other unchanged.
4. Run the control path and verify each consumer uses its intended instance.
5. Repeat after reset and shared-bus load.
6. Boot once with one sensor absent and verify the declared initialization
   policy reaches DEGRADED without a boot loop.

**Expected outcome:** both sensors initialize independently; each sequence
advances only for its acquisition; one state cannot overwrite the other.

**Pass criteria:** two-instance logs, decoded bus transactions and independent
physical readings exist. Address collision or missing sensor is a failure.

### P-16 — Real sensor disconnect and recovery (TST-ACC-02)

**Equipment:** P-15 setup, safe removable connector or inline switch, logic
analyzer, serial capture and oscilloscope.

**Steps:**

1. Run both sensors healthy until stable sequences exist.
2. Disconnect only one sensor without shorting SDA/SCL or power.
3. Record bounded NACK/timeout, debounce, quality and health transition.
4. Verify the other sensor and unrelated tasks meet their budgets.
5. Reconnect and record recovery, cooldown and return to healthy operation.
6. Repeat for the other sensor and for a sensor absent before first appearance.

**Expected outcome:** only the disconnected instance degrades; recovery is
bounded; the other sensor remains independent; reconnected data becomes valid
only after a valid physical acquisition.

### P-17 — Real I2C stuck-low, timeout and nine-clock recovery (TST-ACC-04)

**Equipment:** P-15 setup, open-drain SDA/SCL fault fixture, oscilloscope,
logic analyzer, current-limited supply and removable series resistors.

**Preparation:** validate the fixture powered off. It shall pull a line low
without driving it high and shall have a defined release action.

**Steps:**

1. Capture a healthy transaction for baseline timing.
2. Hold SDA low before a transaction and observe timeout.
3. Release SDA and verify the bounded recovery clock sequence, including up to
   nine pulses as required by the bus contract.
4. Repeat with SCL low and, if supported by the fixture, clock stretching.
5. Inject three consecutive failures and verify cooldown/recovery policy.
6. Keep a second service active and verify its deadlines.

**Expected outcome:** each fault terminates within timeout; recovery is bounded
and observable; the line returns idle-high only after release; no unrelated
service resets.

**Pass criteria:** decoded trace, timeout, pulse count, recovery duration and
post-recovery transaction are recorded. Software NACK alone cannot close this.

### P-18 — Environmental plausibility, calibration and sensor-to-actuator loop

**Equipment:** two real BME280s, calibrated environmental reference or
controlled environment, fan/load, tachometer or airflow reference, DMM,
oscilloscope and serial capture.

**Steps:**

1. Record the stabilized reference environment.
2. Compare temperature, humidity and pressure across declared operating points.
3. Apply an in-range calibration through Cal/XCP and verify the running effect.
4. Persist calibration through NvM and verify it after controlled reset.
5. Remove/invalidate sensor data and verify stale/invalid quality drives the
   fan/display to failsafe.
6. Restore valid data and verify controlled return without oscillation.

**Expected outcome:** values are plausible within tolerance, calibration and
persistence are correct, and the full sensor-to-fan path is safe for valid,
stale and invalid data.

## 5. HW-364A OLED campaign

The following tests are the physical form of TST-OLED-01 through
TST-OLED-08. Existing host or injected-fault results shall be copied into the
record as supporting evidence but shall remain H0/Q0 unless the procedure uses
the real panel and target.

### P-19 — OLED visible orientation and pattern acceptance (TST-OLED-02)

**Equipment:** HW-364A, fitted OLED, camera, serial capture and clean Merlin
image. An upstream reference image may diagnose wiring, but cannot qualify the
Merlin path.

**Steps:**

1. Power-cycle with the panel connected and verify initialization.
2. Display all-off, all-on, checkerboard, corner markers, row markers and the
   normal moving/counter pattern.
3. Confirm origin, row/column order, rotation, color-band arrangement and
   absence of clipping or mirroring.
4. Photograph each static pattern and record a counter sequence.
5. Repeat after at least six resets and one sustained run of at least 30 s.

**Expected outcome:** all patterns and orientation are correct; the counter
changes visibly; serial success alone is not accepted.

**Pass criteria:** the operator signs the visual result and artifacts identify
board, firmware and panel.

### P-20 — OLED chunk, command and complete-frame timing (TST-OLED-03)

**Equipment:** oscilloscope or logic analyzer on SDA/SCL, serial capture,
current meter, clean panel and target image.

**Steps:**

1. Capture initialization commands and every pixel chunk at the declared rate.
2. Measure minimum, average, maximum and percentile duration for at least
   1,000 chunks and at least 100 complete frames.
3. Measure complete-frame latency from first command to final acknowledged
   transfer, including scheduling gaps.
4. Repeat under clean, logging and maximum task load.
5. Compare results to task period, deadline, timeout and recovery budget.

**Expected outcome:** command, chunk and complete-frame durations stay within
the bounded budget; no frame transfer runs inside an RTE critical section.

**Pass criteria:** raw bus trace, decoder settings, sample count, bus speed,
CPU load and complete-frame distribution are retained.

### P-21 — OLED physical NACK, timeout and recovery (TST-OLED-04)

**Equipment:** HW-364A/OLED, logic analyzer, open-drain fault fixture,
optional removable series resistor and serial capture.

**Steps:**

1. Establish a healthy display sequence.
2. Run software NACK injection as a separately labeled baseline.
3. Inject physical NACK/disconnect with a safe removable cable or fixture.
4. Inject SDA-low and SCL-low conditions using the P-17 fixture.
5. Record timeout, retry count, recovery pulse train, cooldown, health and
   resumed redraw.
6. Verify other tasks continue and permanent fault does not create a reset loop.

**Expected outcome:** NACK, timeout and stuck-bus cases are bounded; recovery
is declared and observable; health changes correctly; healthy transfer resumes.

**Pass criteria:** at least one physical result exists for each fault class.
Software NACK-only evidence leaves TST-OLED-04 incomplete.

### P-22 — OLED initialization failure and degraded operation (TST-OLED-05)

**Equipment:** HW-364A, panel cable or address-selection fixture, serial
capture and current meter.

**Steps:**

1. Boot with the panel absent or at a safe wrong address.
2. Verify bounded initialization failure and `DEGRADED` health.
3. Confirm no indefinite transfer, boot loop or false `READY` state.
4. Restore correct panel/address and reboot.
5. Confirm healthy initialization and normal display operation.

**Expected outcome:** missing panel is degraded, dependents follow `initPolicy`
and the ECU remains controllable and diagnosable.

**Pass criteria:** failure/recovery logs, current behavior and reset count are
recorded; wrong-address evidence is not called stuck-bus evidence.

### P-23 — Independent display-instance behavior (TST-OLED-06)

**Equipment:** two physically compatible displays or an approved independent
fixture, logic analyzer and serial capture.

**Preparation:** verify separate addresses and reservations. If the board
cannot safely host two panels, mark physical N/A and retain host evidence.

**Steps:**

1. Start both instances and verify independent health/sequence state.
2. Fault only instance A.
3. Confirm instance B continues its pattern and counters without contamination.
4. Restore A and verify independent recovery.

**Expected outcome:** each display has independent frame ownership, health,
sequence and failure counters.

**Pass criteria:** physical two-instance evidence exists, or the board
limitation and host-only status are explicitly recorded.

### P-24 — OLED steady-state allocation and stack behavior (TST-OLED-07)

**Equipment:** HW-364A, clean image with heap/stack instrumentation and
serial capture; target debugger where available.

**Steps:**

1. Record free heap, minimum free heap, task stack high-water mark and frame
   count after boot.
2. Run at least 1,120 chunk transfers for the declared sustained duration.
3. Record the same values after recovery, reset and resumed redraw.
4. Where the SDK permits, instrument allocator calls and record allocate/free
   pairs instead of relying only on a watermark.

**Expected outcome:** no steady-state leak or unbounded stack reduction occurs;
frame ownership remains bounded; one-time startup allocation is separated from
transfer-path churn.

**Pass criteria:** raw samples and the allocator/stack method are retained.
A flat watermark without allocator-pair analysis is limited evidence.

### P-25 — OLED supervision, skipped activation and SAFE_HALT (TST-OLED-08)

**Equipment:** HW-364A, reset/RTS fixture, serial capture, optional GPIO
marker, current meter and display.

**Steps:**

1. Run the clean display task and verify one watchdog feed per activation after
   the startup gate.
2. Inject a long activation and capture skip re-anchor, deadline count and
   activation delta behavior.
3. Verify a below-TWDT overrun produces deadline evidence while TWDT remains
   silent.
4. Perform five retained resets within 300 s using software and physical
   RTS/reset paths.
5. Verify SAFE_HALT on the next boot, no sixth reset, no display transfer and
   safe output state.
6. Restore clean firmware and clear retained state according to procedure.

**Expected outcome:** supervision is target-real, reset retention works,
SAFE_HALT is stable and the panel task does not run in halt.

**Pass criteria:** boot-by-boot and watchdog/deadline records distinguish
software-reset and physical-reset results.

## 6. v1.1 physical extension campaign

These tests are required when the corresponding v1.1 feature is selected for a
target release. They are not closed by the portable 7A–7L contracts.

### P-26 — Async I2C service-task timing and contention

**Equipment:** physical I2C devices, logic analyzer, oscilloscope, target
instrumentation and a second task/load.

**Steps:** queue multiple transfers from distinct runnables, capture submit,
service, completion and cancellation timing, then repeat with the P-17 fault
fixture. Verify the service task never exceeds its per-activation budget or
synchronous-facade timeout.

**Expected outcome:** queue order, completion ownership, cancellation and
timeout remain bounded under real contention and no unbounded wait or steady-
state heap use appears.

### P-27 — Physical IRQ input and event-task handoff

**Equipment:** signal generator or debounced switch, oscilloscope, logic
analyzer and target image with an IRQ marker.

**Steps:** apply edges at the declared rate, capture ISR entry, notification
and task dispatch, then exceed event capacity. Verify ISR work is bounded and
the event task applies the declared full/drop policy.

**Expected outcome:** callbacks run in task context when deferred, event loss
is counted according to policy and unrelated tasks meet deadlines.

### P-28 — TWAI physical loopback, bus load and bus-off recovery

**Equipment:** supported target pins, approved 3.3 V transceiver, 120 ohm
termination, second CAN node/analyzer and oscilloscope.

**Steps:**

1. Verify transceiver supply, standby/enable levels, CANH/CANL termination and
   pin continuity with power off.
2. Send standard 11-bit and extended 29-bit frames in loopback and to a second
   node; decode identifier, DLC and payload.
3. Fill TX/RX queues and verify bounded full/drop behavior.
4. Apply controlled bus load and measure service budget.
5. Use the approved error fixture to force bus-off; capture detection,
   recovery and re-enable behavior.

**Expected outcome:** frames decode correctly, queue policies are bounded,
bus-off is reported, recovery is explicit and transceiver wiring is safe.

**Pass criteria:** physical frame traces, bit rate, error counters,
termination, current and recovery timing are recorded.

### P-29 — ICU/PCNT pulse frequency and overflow measurement

**Equipment:** calibrated pulse generator or tachometer output, oscilloscope,
logic analyzer and safe level translator if needed.

**Steps:** verify input voltage, GPIO continuity and edge polarity; apply known
pulse frequencies across the declared range; compare count/frequency with the
source; test start, stop, clear and read while pulses continue; exceed the
watch-point range and record overflow; repeat under logging/radio load.

**Expected outcome:** count error remains inside tolerance, lifecycle commands
are deterministic, overflow is reported and missed-edge behavior is measured.

### P-30 — GPTimer release period and jitter

**Equipment:** oscilloscope or frequency counter, GPIO marker, target with
GPTimer release and current meter.

**Steps:**

1. Capture release markers for at least 10,000 periods and target 1,000,000
   periods for final evidence.
2. Measure period, min/max jitter, missed releases and callback/ISR latency.
3. Repeat during flash access, logging, radio traffic and maximum task load.
4. Stop/release the timer and verify no later edge or callback occurs.

**Expected outcome:** GPTimer release meets its timing budget, release/delete
is complete and ISR behavior remains safe under declared interference.

**Pass criteria:** scope trace and statistical distribution are retained;
jitter is not inferred from a host clock.

### P-31 — NvM persistence, corruption and power-loss behavior

**Equipment:** target with release-like flash layout, current-limited power
switch, reset control, serial capture and optional flash reader.

**Steps:**

1. Write a known calibration/configuration record and verify CRC/version.
2. Reset normally and confirm restoration.
3. Interrupt power at controlled points during commit and repeat across the
   commit window.
4. Corrupt a record through the approved test fixture or test image.
5. Verify defaults, RTF-007, quality `INITIAL`, bounded startup and no hang.
6. Repeat within the declared wear-budget limit.

**Expected outcome:** valid data persists; interrupted/corrupt data falls back
to defaults safely; no partial calibration reaches control logic.

### P-32 — Cal/XCP UART physical tuning and persistence

**Equipment:** USB-UART or isolated serial adapter, XCP host script, logic
analyzer, target and NvM setup where selected.

**Steps:**

1. Capture/decode `CONNECT`, `GET_STATUS`, `DOWNLOAD` and `UPLOAD` frames at
   declared framing and baud.
2. Change an in-range parameter and verify the running effect without reboot.
3. Send unknown, malformed, disconnected and out-of-range operations.
4. Reset and verify persistence only when the selected policy requires it.
5. Verify log/XCP multiplexing or dedicated-UART behavior as declared.

**Expected outcome:** frames are unambiguous, invalid operations cannot alter
parameters, accepted changes affect the intended behavior and persistence is
correct.

### P-33 — BswM mode transitions and physical output policy

**Equipment:** target, serial/control input, oscilloscope, PWM/DIO/display
loads and reset control.

**Steps:** issue supported requests for STARTUP, RUN, DEGRADED, SAFE_HALT and
SHUTDOWN. Capture mode, output levels, task activity, watchdog behavior and
reset response. Attempt prohibited transitions after SAFE_HALT and SHUTDOWN.

**Expected outcome:** transitions follow the bounded BswM contract; degraded
mode applies the documented output policy; terminal modes cannot be escaped by
invalid requests; physical outputs are safe.

### P-34 — Cross-core sample transfer under physical system load

**Target:** HW-394 only when dual-core operation is selected.
**Equipment:** oscilloscope/logic analyzer, maximum radio/task/bus load and
serial capture.

**Steps:** run independent producer/consumer cores while radio, logging and
peripheral traffic are active. Record retries, read failures, torn snapshots,
critical-section duration and task deadlines for the declared campaign.

**Expected outcome:** no torn sample/metadata combination, bounded retries and
no load-induced starvation occur.

## 7. Final release gate and sign-off

### 7.1 Required completion order

The operator shall complete tests in this order:

1. P-01/P-02 identity and wiring.
2. P-03/P-04 power, reset and electrical baseline.
3. P-05 through P-14 common target runtime and peripheral behavior.
4. P-19 through P-25 HW-364A OLED behavior when HW-364A is selected.
5. P-15 through P-18 only after exact BME280 hardware is available.
6. P-26 through P-34 for every selected v1.1 feature.
7. Update `docs/measurements.md`, board/driver manifests and `project.lock`
   only from recorded evidence.
8. Re-run host tests, generator fixtures, target builds and QEMU checks after
   evidence-driven changes. These checks supplement, not replace, physical
   evidence.

### 7.2 Release acceptance rules

The release shall remain **unqualified** if any applicable test is blocked,
failed or supported only by host/QEMU evidence. The release record shall list
the exact blocked test and required fixture or hardware.

The following are not physical qualification evidence by themselves:

- QEMU boot or QEMU's absence of I2C device models;
- a host mock, software NACK injection or wrong-address-only run;
- a serial counter without a logic-analyzer/scope trace where timing matters;
- a flat heap watermark without allocator-pair analysis;
- a board sales listing instead of measured pin, pull-up or reset evidence;
- a successful build or flash without observed runtime behavior;
- an upstream OLED example that does not execute Merlin's driver/runtime path.

### 7.3 Final sign-off checklist

- [ ] HW-394 identity/header/pull-up/idle-level evidence complete.
- [ ] HW-364A identity/wiring/address/pull-up evidence complete.
- [ ] Power/current/reset-window/safe-state evidence complete per board.
- [ ] GPIO/UART/I2C/PWM/GPT/ADC/SPI results recorded per board.
- [ ] WLAN result and unsupported Bluetooth result recorded per ECU.
- [ ] Startup, jitter, critical-section, supervision and reset evidence
      recorded per ECU.
- [ ] Real BME280 tests complete, or explicitly deferred to Phase 8.
- [ ] HW-364A TST-OLED-01…08 physical portions complete.
- [ ] Selected TWAI/ICU/GPTimer/NvM/XCP/IRQ tests complete.
- [ ] Every measured value has conditions, sample count, artifact path and
      margin calculation.
- [ ] `docs/measurements.md`, manifests and lock updated from evidence.
- [ ] No physical claim is copied from another ECU, board or simulation.
- [ ] Owner and operator sign-off recorded.
