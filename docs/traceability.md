# Traceability

Requirement IDs are stable. Add new rows without renumbering existing IDs;
each row maps the requirement to its design section, validation rule or runtime
fault (when applicable), and test evidence.

Aligned with [PROJECT_DEFINITION.md](../PROJECT_DEFINITION.md) 2.2.0. Test names
below include current host evidence and acceptance targets. Current
results/limitations are in [measurements.md](measurements.md).
The new modular scope and TST-OLED definitions are in [ecu-support.md](ecu-support.md).

| REQ | Requirement | Design | Validation | Test |
|---|---|---|---|---|
| REQ-ARCH-001 | SWC↔system via RTE only; driver↔HW via MCAL only | §2.1, §7.3 | — | negative compile tests + lint |
| REQ-ARCH-002 | Provider-agnostic ports; ASW includes no driver headers | §2.3, §4 | VAL-024/025 | fixture: swap provider, ASW unchanged |
| REQ-DATA-001 | Coherent per-port sample groups | §6.2 | — | TST-ACC-07 |
| REQ-DATA-002 | Freshness from `sampleTimeUs`, not publication | §6.2 | RTF-004 | TST-ACC-03 |
| REQ-RUN-001 | All timing values tick-representable | §6.1 | VAL-008 | generator fixture |
| REQ-RUN-002 | Deadline miss detectable independent of TWDT | §6.4 | RTF-002 | TST-ACC-05 |
| REQ-RUN-003 | No dynamic allocation in generated/hand-written steady-state control paths (scoped; IDF exception list documented) | §6.1 | — | target allocation tracing |
| REQ-BSW-001 | Init failures recorded and policy-applied before task release | §6.9 | RTF-005 | TST-ACC-06 |
| REQ-BSW-002 | Boot loop terminates in SAFE_HALT | §6.5 | — | TST-ACC-10 |
| REQ-GEN-001 | Byte-identical regeneration from identical lock and inputs | §3.3 | — | CI double-generate |
| REQ-ECU-001 | Separate ECU/SDK capability selection from module/board selection; reject unsupported combinations | §2.2.1, §4.6, §5.5 | VAL-026 | Planned generic ESP8266/no-OLED and wrong-SDK/core/peripheral validation fixtures |
| REQ-ECU-002 | Equivalent common runtime semantics on each qualified backend; no inherited ESP32 API/timing assumptions | §6.10 | VAL-008/026, RTF-002/003/005 | Planned ESP8266 runtime qualification, TST-OLED-08 and common runtime acceptance per ECU |
| REQ-ECU-003 | Reproducible per-target SDK/compiler/configuration and measured hardware limits | §3.3, §4.6, §7.1 | VAL-013/014/026 | Planned per-target build/frozen-lock tests, TST-OLED-01, fixtures #0/#1 |
| REQ-DISP-001 | Reusable SSD1306 driver through RTE/MCAL, independent of ECU and board wiring | §2.3, §7.3 | VAL-006/007/020/024/025/026 | Planned host packing/transport checks, OLED layering negative/positive controls, explicit driver selection on compatible ECUs |
| REQ-DISP-002 | Coherent static frame ownership, bounded chunks/retry/recovery, no steady-state allocation | §6.10 | VAL-021/023, RTF-002/006 | Planned TST-OLED-03/04/06/07 |
| REQ-DISP-003 | Visible OLED acceptance and honest health reporting, including degradable init failure | §4.6, §8.3 | RTF-005/006 | Planned TST-OLED-01/02/04/05 |
| REQ-BOARD-001 | HW-364A auto-adds the ordinary SSD1306 instance and fixed bus resources exactly once | §2.3, §3.2, §5.3 | VAL-001/005/006/015/026 | Planned default expansion/idempotence, pin/address conflict, compatible shared-bus, disable-with-reservations and board-change fixtures |
| REQ-DRV-001 | Device/ECU support advertised only for qualified combinations; generic ESP8266 chooses drivers explicitly | §2.2.1, §4.6, §8.3 | VAL-026 | Planned SSD1306 and BME280 per-backend builds/host tests and physical device records; see compatibility matrix |
