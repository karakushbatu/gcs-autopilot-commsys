#include "flight_logic/FlightLogic.h"
#include "network_layer/NetworkLayer.h"
#include "platform_posix.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace {

const char* kDefaultGcsHost = "127.0.0.1";
const uint16_t kDefaultTcpPort = 5760;
const uint16_t kDefaultUdpPort = 5761;
const char* kDefaultStateFile = "/tmp/autopilot_state.txt";
const int kConnectRetryMs = 500;
const int kHeartbeatIntervalMs = 1000;
const int kTelemetryIntervalMs = 20;
const int kWatchdogCheckMs = 100;
const int kStateWriteMs = 500;
const int kHeartbeatTimeoutMs = 3000;

std::atomic<bool> g_running(true);
std::mutex g_tcp_fd_mutex;
int g_tcp_fd = -1;

std::string g_gcs_host = kDefaultGcsHost;
uint16_t g_tcp_port = kDefaultTcpPort;
uint16_t g_udp_port = kDefaultUdpPort;
std::string g_state_file = kDefaultStateFile;

Autopilot g_autopilot;
NetworkLayer g_network;

int64_t epoch_ms_now() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
               system_clock::now().time_since_epoch())
        .count();
}

std::string iso_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

    std::tm tm_buf;
    platform_localtime(t, &tm_buf);

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void signal_handler(int) {
    g_running.store(false);
    g_autopilot.running.store(false);
}

void parse_args(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--gcs-host" && i + 1 < argc) {
            g_gcs_host = argv[++i];
        } else if (arg == "--tcp-port" && i + 1 < argc) {
            g_tcp_port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--udp-port" && i + 1 < argc) {
            g_udp_port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--state-file" && i + 1 < argc) {
            g_state_file = argv[++i];
        }
    }
}

void write_state_file(AutopilotState state) {
    std::ofstream out(g_state_file.c_str(), std::ios::trunc);
    if (!out) {
        std::cerr << "failed to open state file: " << g_state_file << std::endl;
        return;
    }
    switch (state) {
    case AutopilotState::IDLE:
        out << "state=IDLE\n";
        break;
    case AutopilotState::FLYING:
        out << "state=FLYING\n";
        break;
    case AutopilotState::SAFE_MODE:
        out << "state=SAFE_MODE\n";
        break;
    }
}

bool connect_with_retry() {
    while (g_running.load()) {
        if (g_network.tcp().connect_to(g_gcs_host, g_tcp_port)) {
            g_tcp_fd = g_network.tcp().get_server_fd();
            g_autopilot.last_heartbeat_ms.store(epoch_ms_now());
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(kConnectRetryMs));
    }
    return false;
}

HeartbeatPacket make_heartbeat_packet() {
    HeartbeatPacket hb;
    hb.packet_type = PKT_HEARTBEAT;
    hb.timestamp_ms = host_to_be64(static_cast<uint64_t>(epoch_ms_now()));
    return hb;
}

void tcp_recv_loop() {
    while (g_running.load()) {
        if (g_tcp_fd < 0) {
            if (!connect_with_retry()) {
                return;
            }
        }

        uint8_t packet_type = 0;
        int fd = g_tcp_fd;
        if (!g_network.tcp().recv_all(fd, &packet_type, 1)) {
            std::lock_guard<std::mutex> lock(g_tcp_fd_mutex);
            g_network.tcp().close_fd(g_tcp_fd);
            g_tcp_fd = -1;
            continue;
        }

        if (packet_type == PKT_HEARTBEAT) {
            HeartbeatPacket hb;
            hb.packet_type = packet_type;
            if (!g_network.tcp().recv_all(fd,
                                          reinterpret_cast<uint8_t*>(&hb) + 1,
                                          sizeof(HeartbeatPacket) - 1)) {
                std::lock_guard<std::mutex> lock(g_tcp_fd_mutex);
                g_network.tcp().close_fd(g_tcp_fd);
                g_tcp_fd = -1;
                continue;
            }
            g_autopilot.last_heartbeat_ms.store(epoch_ms_now());
        } else if (packet_type == PKT_CMD_ACK) {
            CommandAck ack;
            ack.packet_type = packet_type;
            if (!g_network.tcp().recv_all(fd,
                                          reinterpret_cast<uint8_t*>(&ack) + 1,
                                          sizeof(CommandAck) - 1)) {
                std::lock_guard<std::mutex> lock(g_tcp_fd_mutex);
                g_network.tcp().close_fd(g_tcp_fd);
                g_tcp_fd = -1;
                continue;
            }
            uint32_t seq = ntohl(ack.sequence_id);
            std::cerr << "[ACK] seq=" << seq
                      << " status=" << static_cast<int>(ack.status) << std::endl;
        } else if (packet_type == PKT_FLIGHT_CMD) {
            FlightModeCommand cmd;
            cmd.packet_type = packet_type;
            g_network.tcp().recv_all(fd,
                                     reinterpret_cast<uint8_t*>(&cmd) + 1,
                                     sizeof(FlightModeCommand) - 1);
        } else {
            std::cerr << "Unknown packet type: "
                      << static_cast<int>(packet_type) << std::endl;
        }
    }
}

void heartbeat_sender_loop() {
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kHeartbeatIntervalMs));
        if (g_tcp_fd < 0) {
            continue;
        }

        HeartbeatPacket hb = make_heartbeat_packet();
        std::lock_guard<std::mutex> lock(g_tcp_fd_mutex);
        if (g_tcp_fd >= 0) {
            if (!g_network.tcp().send_all(g_tcp_fd, &hb, sizeof(hb))) {
                g_network.tcp().close_fd(g_tcp_fd);
                g_tcp_fd = -1;
            }
        }
    }
}

void udp_telemetry_loop() {
    uint32_t tick = 0;
    auto next = std::chrono::steady_clock::now();

    while (g_running.load()) {
        next += std::chrono::milliseconds(kTelemetryIntervalMs);

        TelemetryPacket pkt;
        std::memset(&pkt, 0, sizeof(pkt));
        pkt.packet_type = PKT_TELEMETRY;
        pkt.timestamp_ms = htonl(static_cast<uint32_t>(epoch_ms_now() & 0xFFFFFFFFu));
        pkt.speed_ms = 10.0f + 5.0f * std::sin(static_cast<float>(tick) * 0.1f);
        pkt.altitude_m = 100.0f + static_cast<float>(tick) * 0.01f;
        pkt.checksum = NetworkLayer::compute_xor_checksum(
            reinterpret_cast<const uint8_t*>(&pkt),
            sizeof(TelemetryPacket) - 1);

        g_network.udp().send_to(g_gcs_host, g_udp_port, &pkt, sizeof(pkt));
        ++tick;

        std::this_thread::sleep_until(next);
    }
}

void watchdog_loop() {
    auto last_state_write = std::chrono::steady_clock::now();

    while (g_running.load()) {
        auto now = std::chrono::steady_clock::now();
        int64_t now_ms = epoch_ms_now();
        int64_t last_hb = g_autopilot.last_heartbeat_ms.load();

        if (now_ms - last_hb >= kHeartbeatTimeoutMs &&
            g_autopilot.get_state() != AutopilotState::SAFE_MODE) {
            g_autopilot.set_state(AutopilotState::SAFE_MODE);
            std::cerr << "[" << iso_timestamp()
                      << "] SAFE_MODE ACTIVATED — heartbeat timeout" << std::endl;
            write_state_file(AutopilotState::SAFE_MODE);
        }

        if (now - last_state_write >= std::chrono::milliseconds(kStateWriteMs)) {
            write_state_file(g_autopilot.get_state());
            last_state_write = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(kWatchdogCheckMs));
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    parse_args(argc, argv);
    g_autopilot.running.store(true);
    std::signal(SIGINT, signal_handler);

    write_state_file(AutopilotState::IDLE);

    std::thread tcp_thread(tcp_recv_loop);
    std::thread hb_thread(heartbeat_sender_loop);
    std::thread udp_thread(udp_telemetry_loop);
    std::thread wd_thread(watchdog_loop);

    tcp_thread.join();
    hb_thread.join();
    udp_thread.join();
    wd_thread.join();

    return 0;
}
