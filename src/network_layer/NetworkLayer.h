#ifndef NETWORK_LAYER_H
#define NETWORK_LAYER_H

#include "network_layer/TcpSocket.h"
#include "network_layer/UdpSocket.h"

#include <cstddef>
#include <cstdint>
#include <string>

#define PKT_FLIGHT_CMD   0x01
#define PKT_CMD_ACK      0x02
#define PKT_TELEMETRY    0x03
#define PKT_HEARTBEAT    0x04

#pragma pack(push, 1)

struct FlightModeCommand {
    uint8_t  packet_type;
    uint32_t sequence_id;
    uint8_t  mode;
    uint8_t  reserved[2];
};

struct CommandAck {
    uint8_t  packet_type;
    uint32_t sequence_id;
    uint8_t  status;
};

struct TelemetryPacket {
    uint8_t  packet_type;
    uint32_t timestamp_ms;
    float    speed_ms;
    float    altitude_m;
    uint8_t  checksum;
};

struct HeartbeatPacket {
    uint8_t  packet_type;
    uint64_t timestamp_ms;
};

#pragma pack(pop)

class NetworkLayer {
public:
    NetworkLayer();

    TcpSocket& tcp();
    UdpSocket& udp();

    static uint8_t compute_xor_checksum(const uint8_t* data, size_t len);
    static bool is_ip_in_subnet(const std::string& ip,
                                const std::string& subnet_ip,
                                const std::string& subnet_mask);

private:
    TcpSocket tcp_;
    UdpSocket udp_;
};

#endif
