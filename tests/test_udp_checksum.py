import socket, struct, time, sys

UDP_PORT  = 5761
CTRL_PORT = 5762
PKT_TELEM = 0x03

def make_telemetry(speed, alt, corrupt=False):
    import math
    ts = int(time.time() * 1000) & 0xFFFFFFFF
    body = struct.pack('!BIff', PKT_TELEM, ts, speed, alt)
    checksum = 0
    for b in body:
        checksum ^= b
    if corrupt:
        checksum ^= 0xFF   # flip all bits → guaranteed wrong
    return body + struct.pack('B', checksum)

udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
gcs_addr = ('127.0.0.1', UDP_PORT)

# 50 valid
for i in range(50):
    udp.sendto(make_telemetry(10.0 + i*0.1, 100.0 + i), gcs_addr)
    time.sleep(0.005)

# 50 corrupted
for i in range(50):
    udp.sendto(make_telemetry(10.0, 100.0, corrupt=True), gcs_addr)
    time.sleep(0.005)

udp.close()
time.sleep(0.5)  # let GCS process

# query control port
ctrl = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
ctrl.connect(('127.0.0.1', CTRL_PORT))
ctrl.sendall(b'GET corrupted\n')
resp = ctrl.recv(256).decode().strip()
ctrl.close()

# parse "corrupted_packet_count=N"
count = int(resp.split('=')[1])
if count == 50:
    print("PASS: UDP checksum — 50 corrupted packets dropped correctly")
    sys.exit(0)
else:
    print(f"FAIL: UDP checksum — expected 50 dropped, got {count}")
    sys.exit(1)
