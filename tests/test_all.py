import subprocess, sys, os

scripts = [
    ('TCP Commands (100 ACKs)',  'tests/test_tcp_commands.py'),
    ('UDP Checksum Integrity',   'tests/test_udp_checksum.py'),
    ('Heartbeat Fault Tolerance','tests/test_heartbeat.py'),
    ('ACL Enforcement',          'tests/test_acl.py'),
]

results = []
for name, script in scripts:
    r = subprocess.run([sys.executable, script], capture_output=True, text=True)
    passed = r.returncode == 0
    output = (r.stdout + r.stderr).strip()
    results.append((name, passed, output))
    print(f"{'✓' if passed else '✗'} {name}: {output}")

print("\n─── Summary ───")
all_pass = all(p for _, p, _ in results)
for name, passed, _ in results:
    print(f"  {'PASS' if passed else 'FAIL'}  {name}")

sys.exit(0 if all_pass else 1)
