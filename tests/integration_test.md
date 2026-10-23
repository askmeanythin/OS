# Integration test procedure

1. Build: `make` (Linux) or `scripts\build_windows.bat` (Windows).
2. Start server with simulation + demo data:
   `./bin/exam_server --config config/server.conf --seed-demo --simulate 5`
   (server.conf copied from server.example.conf; default token matches the client default.)
3. In the admin console: `1` list PCs (expect 5 SIM-PC entries, mixed statuses),
   `5` start exam OS-EXAM-1, `7` allocate STU-001 and STU-002,
   `9` view assignments, `10` view alerts, `15` generate report + CSVs.
4. Verify `reports/report_OS-EXAM-1.txt`, `reports/computers.csv`,
   `reports/assignments_OS-EXAM-1.csv`, `reports/alerts.csv` contents against the DB.
5. Exit with `0`; restart the server without flags; `1`/`9` must still show
   the same PCs and assignments (persistence).
6. Real multi-computer variant: follow `docs/setup_guide.md` steps 3–8 on
   2+ Windows PCs and record the outcome (date, PC count, Windows version)
   here before claiming it in the report.
