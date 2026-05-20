import socket, struct, time, sys

TCP_PORT = 5760
PKT_FLIGHT_CMD = 0x01
PKT_CMD_ACK    = 0x02

def send_command(sock, seq, mode):
    # FlightModeCommand: 1+4+1+2 = 8 bytes
    pkt = struct.pack('!BIBBB', PKT_FLIGHT_CMD,
                      seq, mode, 0, 0)
    sock.sendall(pkt)

def recv_ack(sock):
    # CommandAck: 1+4+1 = 6 bytes
    data = b''
    while len(data) < 6:
        chunk = sock.recv(6 - len(data))
        if not chunk:
            return None
        data += chunk
    ptype, seq, status = struct.unpack('!BIB', data)
    return (ptype, seq, status)

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('127.0.0.1', TCP_PORT))

acks = 0
for i in range(1, 101):
    send_command(sock, i, i % 4)
    ack = recv_ack(sock)
    if ack and ack[0] == PKT_CMD_ACK and ack[1] == i and ack[2] == 0:
        acks += 1

sock.close()
if acks == 100:
    print("PASS: TCP — received exactly 100 ACKs for 100 commands")
    sys.exit(0)
else:
    print(f"FAIL: TCP — received {acks}/100 ACKs")
    sys.exit(1)
