# Traceability

Requirement IDs are stable. Add new rows without renumbering existing IDs;
each row maps the requirement to its design section, validation rule or runtime
fault (when applicable), and test evidence.

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
