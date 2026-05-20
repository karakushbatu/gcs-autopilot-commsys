import socket, time, sys, os

CTRL_PORT  = 5762
STATE_FILE = '/tmp/autopilot_state.txt'
TIMEOUT_MS = 3000
TOLERANCE  = 100   # ms

# Tell GCS to stop sending heartbeats
ctrl = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
ctrl.connect(('127.0.0.1', CTRL_PORT))
ctrl.sendall(b'STOP_HEARTBEAT\n')
ctrl.recv(64)
ctrl.close()

t0 = time.time()
safe_mode_detected_ms = None

# Poll state file for up to 5 seconds
deadline = t0 + 5.0
while time.time() < deadline:
    try:
        with open(STATE_FILE) as f:
            content = f.read().strip()
        if 'SAFE_MODE' in content:
            elapsed_ms = (time.time() - t0) * 1000
            safe_mode_detected_ms = elapsed_ms
            break
    except FileNotFoundError:
        pass
    time.sleep(0.05)

# Resume heartbeats for subsequent tests
ctrl = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
ctrl.connect(('127.0.0.1', CTRL_PORT))
ctrl.sendall(b'RESUME_HEARTBEAT\n')
ctrl.recv(64)
ctrl.close()

if safe_mode_detected_ms is None:
    print("FAIL: Heartbeat — SAFE_MODE not detected within 5 seconds")
    sys.exit(1)

lo = TIMEOUT_MS - TOLERANCE
hi = TIMEOUT_MS + TOLERANCE
if lo <= safe_mode_detected_ms <= hi:
    print(f"PASS: Heartbeat — SAFE_MODE in {safe_mode_detected_ms:.0f}ms "
          f"(expected {lo}–{hi}ms)")
    sys.exit(0)
else:
    print(f"FAIL: Heartbeat — SAFE_MODE in {safe_mode_detected_ms:.0f}ms "
          f"(expected {lo}–{hi}ms)")
    sys.exit(1)
