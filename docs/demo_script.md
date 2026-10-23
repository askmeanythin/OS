# Faculty Demonstration Script (~10 minutes)

**Setup before the demo:** build done (`scripts\build_windows.bat`), server
PC + at least 1 client PC on the lab LAN (or simulation mode as fallback —
say so openly if used).

1. **Problem (1 min).** "Before an exam, staff check every lab PC by hand —
   slow, and failures are found by students. Our system automates health
   checks, allocation, monitoring and reporting."
2. **Start the server** (`scripts\run_server.bat`). Point at the console:
   SQLite DB auto-created, listening on port 5050.
3. **Start a client** on another PC (`scripts\run_client.bat`). On the server
   press `1` — the PC appears with live CPU/RAM from real Windows APIs.
4. **Health rules.** Explain READY/WARNING/NOT_READY/DISCONNECTED and the
   configurable thresholds (85% CPU / 90% RAM are demo values in config).
5. **Exam flow.** Menu `16` (seed demo students + exam), `5` start
   `OS-EXAM-1`, `7` allocate `STU-001` → system picks the lowest-load READY
   PC; allocate again for the same student → rejected (duplicate-safe).
6. **Failure drill.** Kill the client (Ctrl+C) → within the heartbeat
   timeout the PC flips to DISCONNECTED and an alert is raised (menu `10`).
   Restart the client → it reconnects, alert auto-resolves.
7. **Maintenance.** Menu `14` on a PC → it is excluded from allocation.
8. **Reports.** Menu `6` end exam, `15` generate report → open
   `reports\report_OS-EXAM-1.txt` and a CSV in a spreadsheet.
9. **Close (1 min).** OS concepts mapping (see report §5), limitations:
   LAN-only plain TCP, Windows metrics tested via build + simulation,
   real multi-PC run documented in the setup guide.
