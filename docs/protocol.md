# Wire Protocol (v1)

## Framing
Every message on the TCP stream is framed as:

```
+--------------------------------+----------------------+
| 4-byte length (big-endian)     | payload (ASCII)      |
+--------------------------------+----------------------+
```
Length must be 1..1024. A receiver loops on `recv()` until the full header
and then the full payload have arrived (partial reads are normal on TCP) and
loops on `send()` until all bytes are written. A peer that sends an invalid
length or a malformed payload is disconnected; the server keeps running.

## Payload format
`1|<TYPE>|<pc_id>|<epoch_seconds>|<key=value;key=value;...>`

Raw C structs are never sent (padding/endianness would corrupt the format).

## Message types
| Type | Direction | Body keys |
|---|---|---|
| REGISTER | client → server | token, host, cpu, ram, net, sw, procs |
| HEARTBEAT / HEALTH_UPDATE | client → server | same as REGISTER |
| ACK | server → client | ok=1 |
| ERROR | server → client | reason=bad_token / bad_metrics / unsupported_type |
| SHUTDOWN | client → server | token |

## Validation rules
- Protocol version must be `1`.
- `pc_id`: 1–31 chars of `[A-Za-z0-9_-]`.
- cpu/ram must be within 0–100, else `ERROR bad_metrics`.
- Every client message must carry the provisioning `token`; wrong token →
  `ERROR bad_token` and no state change.
- Heartbeat freshness: server marks a PC DISCONNECTED when
  `now - last_seen > heartbeat_timeout_s` (sweeper thread, every 2 s).

## Example
```
REGISTER:  1|REGISTER|PC-01|1760000000|token=***;host=LAB1-PC01;cpu=23.0;ram=41.0;net=1;sw=1;procs=132
ACK:       1|ACK|PC-01|1760000000|ok=1
```

## Security notes
Plain TCP provides **no encryption** — this is a LAN-only college prototype.
Run it only on a trusted lab network, bind/firewall as described in
`setup_guide.md`, and use a per-lab token that is not committed to source
control. For production, the same framing can be carried over TLS (e.g.
OpenSSL/mbedTLS) without changing the payload format.
