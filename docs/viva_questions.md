# Viva Questions & Answers (30)

1. **Why TCP and not UDP?** Allocation/health state needs reliable, ordered delivery; TCP gives that. UDP would need us to rebuild reliability for state messages.
2. **Why length-prefix framing?** TCP is a byte stream — one `recv()` is not one message. The 4-byte big-endian length tells the receiver exactly how many bytes form one message; partial reads are looped over.
3. **Why not send C structs directly?** Padding, alignment and endianness differ across compilers/machines; an explicit text format is portable and debuggable.
4. **How is CPU % calculated?** From `GetSystemTimes` deltas over a 250 ms interval: 100 × (total − idle)/total. A single instantaneous sample would be meaningless.
5. **How is RAM usage read?** `GlobalMemoryStatusEx` → `dwMemoryLoad` and available physical memory.
6. **How do you detect a dead client?** Heartbeats every 5 s (configurable); a sweeper thread marks PCs DISCONNECTED when `now − last_seen > timeout` (30 s) and raises an alert.
7. **How are duplicate assignments prevented?** `BEGIN IMMEDIATE` transaction around select+insert, plus partial unique indexes on active assignments per PC and per (student, exam).
8. **Allocation policy and complexity?** Lowest `cpu+ram` score among fresh READY PCs, tie-break by pc_id: O(P log P) for P PCs.
9. **Allocation vs CPU scheduling — difference?** We allocate whole PCs (resources) to students; the OS kernel schedules processes/threads on CPUs. We monitor CPU, we don't schedule it.
10. **Which OS concepts does the project show?** Process observation, memory statistics, CPU utilization, resource allocation, file persistence, networking, client-server scheduling of work.
11. **Why SQLite?** Embedded, zero-admin, transactional, single-file — ideal for one server PC. Accessed only via prepared statements (no SQL injection).
12. **What is a prepared statement?** SQL pre-compiled with bound parameters; data can never be reinterpreted as SQL.
13. **What does `PRAGMA foreign_keys=ON` do?** Enforces referential integrity (SQLite defaults it off per connection).
14. **How are alerts deduplicated?** A partial unique index allows only one OPEN/ACKED alert per (pc, category); recovery auto-resolves.
15. **What happens if the server restarts mid-exam?** The DB is on disk (WAL mode); state survives. Clients reconnect automatically with backoff and resume heartbeats.
16. **What if two clients use the same pc_id?** They would overwrite each other's record — prevented operationally by unique config per PC; documented limitation, server rejects nothing because IDs are provisioned, token identifies the lab.
17. **How is the client authenticated?** A per-lab provisioning token in every message, checked server-side. It's a college prototype control, not a production credential system.
18. **Is the traffic encrypted?** No — plain TCP, LAN-only by design; docs recommend TLS for production and give the firewall procedure.
19. **Why a thread per client?** Simple and a slow client can't block others; DB access is serialized with one lock, which is fine at lab scale (~60 PCs).
20. **Race condition you actually solved?** Two allocations taking the same PC — solved by transaction + unique index, not by hoping timing works out.
21. **What is heartbeat staleness vs disconnection?** A closed socket is seen immediately only by that client's handler; other failure modes (power loss, cable) leave no close event — only the timestamp check catches them.
22. **Why configurable thresholds?** 85%/90% are demonstration values, not universal hardware truths; labs differ, so they're config, not constants.
23. **What does the client collect — and not collect?** Only CPU/RAM/process-count/hostname/software presence. Never keystrokes, files, screenshots or passwords.
24. **How does simulation mode differ from real mode?** Same protocol, same server path, same TCP — only the metrics are generated (deterministically) instead of read from the OS. It's workflow evidence, not hardware evidence.
25. **What is WAL mode?** Write-ahead logging: readers don't block the writer; good for concurrent client threads + admin console.
26. **How are CSVs made spreadsheet-safe?** Fields containing commas/quotes/newlines are quoted with doubled quotes per RFC 4180, headers always written.
27. **Partial writes — handled?** `proto_send` loops until all bytes are written or an error occurs.
28. **What if a client sends garbage?** Parse validation fails → connection dropped, server unaffected; oversized frames rejected by the length check.
29. **How would you scale to 500 PCs?** Move from thread-per-client + single lock to an event loop / connection pool and batched health inserts; protocol and schema already support it.
30. **Biggest limitation?** No transport encryption and single-server design; both are documented with the production path (TLS, and a replicated DB) in the report.
