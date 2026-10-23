# Testing

## Automated unit tests (run with `make test`)
| Suite | Covers | Result (dev machine, Linux build of the same sources) |
|---|---|---|
| `tests/test_health.c` | READY / high-CPU WARNING / high-RAM WARNING / stale DISCONNECTED / missing-software NOT_READY / net-down NOT_READY / ASSIGNED / MAINTENANCE / precedence | ALL PASS (9 cases) |
| `tests/test_protocol.c` | build+parse round-trip, kv lookup, wrong version, unknown type, bad pc_id, garbage, oversized payload | ALL PASS |
| `tests/test_database.c` | schema creation, computer upsert, student/exam CRUD, health insert, **persistence across close/reopen** | ALL PASS |
| `tests/test_allocation.c` | exam-not-active, unknown student, lowest-load deterministic pick, stale PC skipped despite best metrics, duplicate student rejected, assigned PC skipped, high CPU/high RAM/missing software excluded → NO_PC_AVAILABLE, release + reassignment | ALL PASS |

The Windows executables are built from the identical sources
(`scripts/build_windows.bat` natively, or `make windows` cross-compiled).
Re-run `make test` after any change; do not edit results into this file by hand.

## Integration test actually performed
On the development machine (Linux build, same C sources), 2026-10-23:
1. Server started in simulation mode with 5 simulated agents over loopback
   TCP. All 5 registered; the deterministic hot-PC scenario appeared as
   WARNING (93% CPU) and raised a HIGH_CPU alert.
2. Exam OS-EXAM-1 created/started via the admin console; allocations followed
   the documented policy exactly — STU-001 → SIM-PC-05 (lowest load),
   STU-002 → SIM-PC-01 (next lowest) — visible in the assignment history.
3. Report + all 3 CSV exports generated and inspected: report totals match
   the database (2 active allocations, 4 unassigned students listed).
4. Server stopped and restarted: PCs and active assignments persisted.
5. A **real** (non-simulated) client agent run: registered as LAB-PC-REAL-01
   with the machine's real hostname, CPU 4.0% and RAM 81.9% read from /proc,
   health rows stored in the DB.

## NOT tested (honest scope)
- Real multi-computer LAN run on Windows — the deployment is implemented and
  cross-compiled, and `docs/setup_guide.md` gives the exact procedure, but it
  had **not** been executed on physical lab PCs at the time of writing.
  Simulation results must never be quoted as multi-computer evidence.
- Windows-native metric values (GetSystemTimes/GlobalMemoryStatusEx paths)
  compile under MinGW but were not executed on Windows hardware here.
- TLS transport (documented limitation, LAN-only prototype).

## Manual test matrix for the lab session
Invalid config values · oversized/garbage network message (client dropped,
server survives) · client killed mid-exam (DISCONNECTED + alert within the
timeout) · server restart mid-exam (data persists, clients reconnect) ·
two allocations racing for the last PC (exactly one wins) · CSV opened in
Excel/LibreOffice (headers, quoting) · repeated start/stop cycles.
