# Smart Exam Lab Management System

An Operating Systems project in **C**: a central server monitors a computer
lab over TCP/IP, checks every PC's health (CPU, RAM, network, required
software), allocates PCs to students with a deterministic, race-safe
algorithm, raises alerts, and generates exam reports from a SQLite database.

**Team:** Rajesh Kumar Gautam (24BCE0326) · Shourya Thakur (24BCI0171) ·
Priyanshu Gautam (24BCE0317) — School of Computer Science and Engineering.
Submitted to: Prof. Geraldine Bessie Amali, SCOPE.

## Requirements
- **OS:** Windows 10/11 (64-bit) for the lab deployment. The same C sources
  also build and run on Linux for development/testing.
- **Compiler:** MinGW-w64 GCC (for building from source). Easiest install:
  MSYS2 from https://www.msys2.org, then in the *MSYS2 MinGW 64-bit* terminal:
  `pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make`
- **No other libraries to install:** SQLite is bundled as the official
  amalgamation in `third_party/sqlite/` and Winsock2 ships with Windows.
- **Network:** all PCs on the same LAN/subnet; server port 5050/TCP must be
  allowed through Windows Firewall on the server PC only (narrow rule in
  `docs/setup_guide.md` — never disable the firewall globally).
- **Prebuilt executables** are in `bin/` (`exam_server.exe`, `exam_client.exe`)
  if you do not want to compile.

## How to run — step by step
**Option A — one PC demo (simulation):**
1. `scripts\build_windows.bat` (skip if using the prebuilt `bin\*.exe`)
2. `scripts\run_simulation.bat` — server + 8 simulated PC agents start.
3. In the admin console: press `16` (seed demo students + exam), `5` then
   `OS-EXAM-1` (start exam), `7` then `OS-EXAM-1` / `STU-001` (allocate),
   `1` to view the live PC dashboard, `15` to generate the report + CSVs
   into `reports\`, `0` to exit.

**Option B — real lab (multiple Windows PCs):**
1. Build once (above), copy this whole folder to every lab PC.
2. On the **server PC**: `ipconfig` → note the IPv4 (e.g. 192.168.1.10).
   Copy `config\server.example.conf` → `config\server.conf`, set a fresh
   `lab_token`, and add the firewall rule from `docs/setup_guide.md` §5.
3. On **each client PC**: copy `config\client.example.conf` →
   `config\client.conf`; set a **unique** `pc_id` (PC-01, PC-02, ...),
   `server_host` = server IPv4, and the **same** `lab_token`.
4. Start the server: `scripts\run_server.bat` (real mode:
   `bin\exam_server.exe --config config\server.conf`).
5. Start each client: `scripts\run_client.bat`
   (or `bin\exam_client.exe --config config\client.conf`).
6. Verify in the admin console with `1` (all PCs READY, last-seen updating),
   then run the exam flow from Option A steps 3.
7. Full verification checklist + troubleshooting: `docs/setup_guide.md` §8.

**Tests (Linux or MSYS2):** `make test` runs all four unit suites
(health, protocol, database, allocation).

## Quick start (Windows)
```
scripts\build_windows.bat          :: needs MinGW-w64 GCC on PATH (MSYS2 works)
copy config\server.example.conf config\server.conf
copy config\client.example.conf config\client.conf   :: set server IP + unique pc_id + same lab_token
scripts\run_server.bat             :: server + admin console (simulation demo)
scripts\run_client.bat             :: on each lab PC
```
Full multi-computer walkthrough (firewall rule, IP setup, verification
checklist): **docs/setup_guide.md**.

## Modes
- **Real mode:** `bin\exam_server.exe` on one PC, `bin\exam_client.exe` on
  each lab PC. Agents report real Windows metrics
  (`GetSystemTimes`, `GlobalMemoryStatusEx`, ToolHelp32 process count).
- **Simulation mode:** `bin\exam_server.exe --simulate 8 --seed-demo` runs 8
  simulated agents over loopback TCP on one PC. Metrics are simulated and
  labelled as such — simulation is not multi-computer test evidence.

## Build (Linux/dev + cross-compile)
```
make            :: Linux binaries + tests
make test       :: run all unit tests
make windows    :: cross-compile the Windows .exe files (MinGW-w64)
```
SQLite is bundled as the official amalgamation in `third_party/sqlite/`
(single `sqlite3.c`), so no separate database installation is needed anywhere.

## Docs
- `docs/architecture.md` — modules, health rules, allocation pseudocode
- `docs/protocol.md` — wire protocol v1
- `docs/setup_guide.md` — Windows deployment & troubleshooting
- `docs/testing.md` — what was tested, and what honestly was not
- `docs/demo_script.md` — faculty demo walkthrough
- `docs/viva_questions.md` — 30 viva Q&A

## Status vocabulary
READY · WARNING · NOT_READY · DISCONNECTED · ASSIGNED · MAINTENANCE —
precedence rules in `docs/architecture.md`. Thresholds (demo defaults:
85% CPU, 90% RAM, 30 s heartbeat timeout) live in the config files.
