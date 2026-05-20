# Secure & Fault-Tolerant GCS ↔ Autopilot Communication

A two-process C++11 system that simulates secure ground-control and autopilot
communication over TCP (commands and heartbeats) and UDP (high-rate telemetry).
The design separates raw socket I/O (`NetworkLayer`) from flight state and
telemetry handling (`FlightLogic`).

## Build

```bash
cmake -B build && cmake --build build
```

Requires CMake 3.16+, a C++11 compiler, and pthread (Linux).

## Run

Terminal 1 — GCS (server):

```bash
./build/gcs --allow-localhost &
```

Terminal 2 — Autopilot (client):

```bash
./build/autopilot &
```

Optional flags: `--tcp-port`, `--udp-port`, `--ctrl-port`, `--subnet`,
`--allow-localhost` (GCS); `--gcs-host`, `--state-file` (autopilot).

## Test

With GCS and autopilot running:

```bash
python3 tests/test_all.py
```

Individual scripts are listed below. The ACL test requires restarting GCS with a
restrictive subnet (see CI workflow).

## Architecture

```
┌─────────────────┐                      ┌──────────────────┐
│       GCS       │                      │    Autopilot     │
│    (server)     │                      │    (client)      │
├─────────────────┤                      ├──────────────────┤
│ TCP :5760       │◄── Heartbeat ───────►│ TCP connect      │
│                 │◄── FlightModeCmd ────│ (from tests)     │
│                 │─── CommandAck ──────►│                  │
│ UDP :5761       │◄── Telemetry 50Hz ───│ UDP send         │
│ CTRL :5762      │◄── test queries ─────│                  │
└─────────────────┘                      └──────────────────┘
        │                                         │
        │  /tmp/gcs_stats.txt                     │  /tmp/autopilot_state.txt
        └─────────────────────────────────────────┘
```

## Modules

| Module | Responsibility |
|--------|----------------|
| **NetworkLayer** | `TcpSocket`, `UdpSocket`, packed packet structs, XOR checksum, subnet ACL helper |
| **FlightLogic** | `Autopilot` state machine and `GCSLogic` telemetry validation / stats |

## Packet reference

| Type | ID | Struct | Size (bytes) | Fields |
|------|-----|--------|--------------|--------|
| Flight command | `0x01` | `FlightModeCommand` | 8 | `packet_type`, `sequence_id` (BE), `mode`, `reserved[2]` |
| Command ACK | `0x02` | `CommandAck` | 6 | `packet_type`, `sequence_id` (BE), `status` |
| Telemetry | `0x03` | `TelemetryPacket` | 14 | `packet_type`, `timestamp_ms` (BE), `speed_ms`, `altitude_m`, `checksum` (XOR) |
| Heartbeat | `0x04` | `HeartbeatPacket` | 9 | `packet_type`, `timestamp_ms` (BE u64) |

Multi-byte integers use network byte order (`htonl` / `htobe64`). IEEE-754 floats
are sent host-endian (tests and autopilot run on the same host).

## Threading model

**GCS**

| Thread | Role |
|--------|------|
| TCP accept | Listen on `--tcp-port`, ACL check, spawn per-client handler |
| Per-client TCP | Dispatch heartbeat / flight commands |
| Heartbeat sender | Every 1 s, send heartbeat to autopilot TCP session |
| UDP receiver | Validate telemetry checksum, update `GCSLogic` |
| Stats writer | Every 1 s, write `/tmp/gcs_stats.txt` |
| Control port | TCP on `--ctrl-port` for `GET corrupted`, `STOP/RESUME_HEARTBEAT` |

**Autopilot**

| Thread | Role |
|--------|------|
| TCP recv | Connect/reconnect, handle heartbeat / ACK packets |
| Heartbeat sender | Every 1 s, send heartbeat to GCS |
| UDP telemetry | 50 Hz mock telemetry with `sleep_until` pacing |
| Watchdog | Every 100 ms, 3 s heartbeat timeout → `SAFE_MODE`; state file every 500 ms |

## Tests

| Script | Proves |
|--------|--------|
| `test_tcp_commands.py` | 100 flight commands receive matching ACKs |
| `test_udp_checksum.py` | 50 bad checksums increment `corrupted_packet_count` |
| `test_heartbeat.py` | GCS heartbeat stop triggers autopilot `SAFE_MODE` ~3 s |
| `test_acl.py` | Non-subnet client (127.0.0.1) rejected when ACL is strict |
| `test_all.py` | Runs all four (ACL needs manual GCS restart or CI) |

Run individually:

```bash
python3 tests/test_tcp_commands.py
python3 tests/test_udp_checksum.py
python3 tests/test_heartbeat.py
python3 tests/test_acl.py   # after restarting GCS with --subnet 192.168.2.0/24
```

## Known limitations

- ACL test requires restarting GCS without `--allow-localhost` and a different
  `--subnet`; CI automates this.
- TCP `accept` blocks; shutdown waits until a connection attempt or signal.
- One autopilot TCP session is tracked for outbound GCS heartbeats; additional
  clients (e.g. test scripts) use separate threads.
- Corrupted-packet counter is cumulative across a GCS process lifetime.
