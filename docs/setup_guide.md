# Setup & Multi-Computer Deployment Guide (Windows 10/11)

## 1. What you need
- 2+ Windows PCs on the **same lab LAN** (same switch/router, same subnet).
- On the build PC: MinGW-w64 GCC. Easiest: install **MSYS2**
  (https://www.msys2.org), open the *MSYS2 MinGW 64-bit* terminal and run
  `pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make`.
- SQLite is **bundled** (`third_party/sqlite/sqlite3.c`) — nothing else to install.

## 2. Build (on one PC)
```
cd SmartExamLab\scripts
build_windows.bat
```
Produces `bin\exam_server.exe` and `bin\exam_client.exe`. Copy the whole
`SmartExamLab` folder (or at least `bin\` + `config\`) to each client PC —
the same `.exe` runs everywhere; only the config differs.

## 3. Find the server's LAN IPv4
On the server PC: `ipconfig` → note the IPv4 address, e.g. `192.168.1.10`.
(Addresses differ per lab — never hard-code someone else's.)

## 4. Configure the server
Copy `config\server.example.conf` to `config\server.conf` and set:
- `bind_addr=0.0.0.0` (or the server's LAN IP to bind only that interface)
- `port=5050`
- `lab_token=` a fresh token you invent for your lab (keep it private,
  don't commit it).

## 5. Windows Firewall (server PC only, narrow rule)
Do **not** disable the firewall. Add one inbound rule for just this port:
```
netsh advfirewall firewall add rule name="SmartExamLab Server 5050" dir=in action=allow protocol=TCP localport=5050 profile=private
```
Run it in an *Administrator* Command Prompt. Institutional labs may require
IT permission — ask; do not bypass their controls. Remove later with:
`netsh advfirewall firewall delete rule name="SmartExamLab Server 5050"`

## 6. Configure each client
Copy `config\client.example.conf` to `config\client.conf` on each PC:
- `pc_id=` unique per PC (`PC-01`, `PC-02`, ...)
- `server_host=` the server's IPv4 from step 3
- `port=5050`, `lab_token=` identical to the server's
- optional `required_software=` e.g. `code.exe,chrome.exe`

## 7. Run
Server PC: `scripts\run_server.bat` (or `bin\exam_server.exe --config config\server.conf`).
Each client PC: `scripts\run_client.bat`.

## 8. Verify (checklist)
1. Admin console → `1` (PCs): every client appears, status READY, last-seen updating.
2. Menu `16` seed demo data, `5` start exam `OS-EXAM-1`, `7` allocate a student → a PC is assigned.
3. Unplug/stop one client → within ~30 s it flips to DISCONNECTED and a
   DISCONNECTED alert appears (menu `10`). Restart it → it re-registers and the alert resolves.
4. Menu `15` → report + CSVs appear in `reports\`.
5. Stop everything with Ctrl+C / menu `0`.

## 9. One-PC simulation (for practice / viva rehearsal)
`scripts\run_simulation.bat` — starts the server plus 8 simulated agents on
loopback. Labelled simulation: good for workflow demos, **not** evidence of
multi-computer testing.

## Troubleshooting
| Symptom | Likely cause |
|---|---|
| Client never appears | wrong `server_host`, firewall rule missing, port mismatch |
| `bad_token` in behaviour | client/server `lab_token` differ |
| Bind failed on server | another process uses 5050 → change `port` everywhere |
| PC flips DISCONNECTED often | heartbeat timeout too small for a slow LAN → raise both values |
| Antivirus prompt | allow the two `.exe` files you built; do not disable AV globally |
