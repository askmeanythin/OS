# Architecture — Smart Exam Lab Management System

## Overview
One Windows PC runs the **server** (`exam_server.exe`): TCP listener, SQLite
database, health evaluation, allocation engine, alerts, reports and the
administrator console. Every lab PC runs the **client agent**
(`exam_client.exe`), which measures its own machine and reports to the server
over the lab LAN. In **simulation mode** the server spawns simulated agents
in-process that connect over real loopback TCP — the full network path is
exercised, but the *metrics* are simulated and labelled as such.

```
 [PC-01 agent] --\
 [PC-02 agent] ---- TCP 5050 (length-prefixed protocol) ---> [Server]
 [PC-03 agent] --/        | SQLite (examlab.db)
                          | Admin console (terminal menu)
                          +-> reports/*.txt, *.csv
```

## Modules
| File | Responsibility |
|---|---|
| `src/common.c` | shared types, time/string helpers |
| `src/config.c` | key=value config files (thresholds, port, token) |
| `src/protocol.c` | length-prefixed framing, safe parse/validate, kv extraction |
| `src/database.c` | SQLite schema + prepared-statement CRUD, DB-wide lock |
| `src/monitor.c` | Windows/Linux metrics, simulation backend, health rules |
| `src/allocation.c` | deterministic eligible-PC selection in a transaction |
| `src/alerts.c` | deduplicated alert raise/ack/resolve |
| `src/reports.c` | exam report + CSV exports from stored data |
| `src/server.c` | thread-per-client server, heartbeat sweeper, simulation launcher |
| `src/client.c` | agent loop: register, heartbeat, reconnect with backoff |
| `src/main_server.c` | admin console menu |
| `src/main_client.c` | agent entry point / CLI flags |

## Health evaluation
Status precedence (highest first): **DISCONNECTED** (heartbeat older than
`heartbeat_timeout_s`) > **MAINTENANCE** > **NOT_READY** (network down or
required software missing) > **ASSIGNED** > **WARNING** (CPU ≥ `cpu_warn_pct`
or RAM ≥ `ram_warn_pct`) > **READY**. Implemented as the pure function
`health_evaluate()` in `monitor.c` and unit-tested in `tests/test_health.c`.

## Allocation algorithm
```
ALLOCATE(exam, student):
  1. student must exist; exam must exist and be ACTIVE
  2. BEGIN IMMEDIATE                      -- takes DB write lock
  3. reject if student already has an active assignment in this exam
  4. candidates = computers with no active assignment, not in maintenance,
     whose last health record is fresh (age <= freshness_s) and whose
     evaluated status is READY
  5. score(pc) = cpu_pct + ram_pct; pick minimum; tie-break pc_id ascending
  6. INSERT assignment; mark PC ASSIGNED; COMMIT
```
Complexity: candidate scan O(P) over P computers + sort O(P log P); all other
steps are indexed lookups. Atomicity comes from `BEGIN IMMEDIATE` plus the
partial unique indexes `ux_active_pc` / `ux_active_student` — two concurrent
requests can never receive the same PC.

## OS concepts demonstrated
- **Process management** — the agent counts running processes
  (ToolHelp32 snapshot on Windows, `/proc` scan on Linux) and reports it;
  the server uses one thread per connection. The project *observes*
  processes; it does not implement an OS scheduler.
- **Memory management** — `GlobalMemoryStatusEx` gives RAM load/available.
- **CPU management** — utilization from `GetSystemTimes` deltas over an
  interval (250 ms), never a single instantaneous sample. Monitoring
  utilization is not CPU scheduling; thresholds only classify PCs.
- **Resource management / scheduling & allocation** — PCs are the managed
  resources; the allocation policy above reserves them. This is resource
  allocation, distinct from process scheduling inside the OS.
- **File management** — SQLite database, config files, logs/exports/reports.
- **Networking** — TCP client-server, framing, heartbeats, timeouts,
  reconnection. Note on ping: ICMP success only shows a host answers; it
  says nothing about exam services — the authoritative liveness check here
  is the agent's actual TCP session with the server.
