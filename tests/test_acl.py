# ACL test: restart GCS with a different subnet so 127.0.0.1 is rejected.
# In CI, we pass --subnet 192.168.2.0/24 (no --allow-localhost) to gcs,
# then connect from 127.0.0.1 and expect immediate close.
import socket, time, sys

TCP_PORT = 5760

t0 = time.time()
try:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(1.0)
    sock.connect(('127.0.0.1', TCP_PORT))
    # Try to recv — should get empty (connection closed) fast
    data = sock.recv(16)
    elapsed_ms = (time.time() - t0) * 1000
    sock.close()
    if data == b'' and elapsed_ms < 50:
        print(f"PASS: ACL — unauthorized connection closed in {elapsed_ms:.1f}ms")
        sys.exit(0)
    else:
        print(f"FAIL: ACL — got data or took too long ({elapsed_ms:.1f}ms, data={data})")
        sys.exit(1)
except ConnectionRefusedError:
    elapsed_ms = (time.time() - t0) * 1000
    if elapsed_ms < 50:
        print(f"PASS: ACL — connection refused in {elapsed_ms:.1f}ms")
        sys.exit(0)
    else:
        print(f"FAIL: ACL — refused but too slow ({elapsed_ms:.1f}ms)")
        sys.exit(1)
except Exception as e:
    print(f"FAIL: ACL — unexpected error: {e}")
    sys.exit(1)
