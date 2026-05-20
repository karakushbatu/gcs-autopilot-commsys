#include "flight_logic/FlightLogic.h"
#include "network_layer/NetworkLayer.h"
#include "platform_posix.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace {

const uint16_t kDefaultTcpPort = 5760;
const uint16_t kDefaultUdpPort = 5761;
const uint16_t kDefaultCtrlPort = 5762;
const int kHeartbeatIntervalMs = 1000;
const int kStatsIntervalMs = 1000;
const int kAcceptPollMs = 100;

std::atomic<bool> g_running(true);
std::atomic<bool> g_send_heartbeat(true);
std::atomic<int> g_autopilot_fd(-1);
std::mutex g_autopilot_fd_mutex;

uint16_t g_tcp_port = kDefaultTcpPort;
uint16_t g_udp_port = kDefaultUdpPort;
uint16_t g_ctrl_port = kDefaultCtrlPort;
std::string g_subnet_ip = "192.168.1.0";
std::string g_subnet_mask = "255.255.255.0";
std::string g_subnet_cidr = "192.168.1.0/24";
bool g_allow_localhost = false;

GCSLogic g_gcs_logic;
NetworkLayer g_network;

int64_t epoch_ms_now() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
               system_clock::now().time_since_epoch())
        .count();
}

void signal_handler(int) {
    g_running.store(false);
}

bool parse_cidr(const std::string& cidr, std::string& out_ip, std::string& out_mask) {
    size_t slash = cidr.find('/');
    if (slash == std::string::npos) {
        return false;
    }
    out_ip = cidr.substr(0, slash);
    int prefix = std::atoi(cidr.substr(slash + 1).c_str());
    if (prefix < 0 || prefix > 32) {
        return false;
    }
    uint32_t mask = (prefix == 0) ? 0 : (0xFFFFFFFFu << (32 - prefix));
    in_addr addr;
    addr.s_addr = htonl(mask);
    char buf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &addr, buf, sizeof(buf)) == NULL) {
        return false;
    }
    out_mask = buf;
    return true;
}

void parse_args(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--allow-localhost") {
            g_allow_localhost = true;
        } else if (arg == "--tcp-port" && i + 1 < argc) {
            g_tcp_port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--udp-port" && i + 1 < argc) {
            g_udp_port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--ctrl-port" && i + 1 < argc) {
            g_ctrl_port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--subnet" && i + 1 < argc) {
            g_subnet_cidr = argv[++i];
            parse_cidr(g_subnet_cidr, g_subnet_ip, g_subnet_mask);
        }
    }
}

bool is_whitelisted(const std::string& ip) {
    if (g_allow_localhost && ip == "127.0.0.1") {
        return true;
    }
    return NetworkLayer::is_ip_in_subnet(ip, g_subnet_ip, g_subnet_mask);
}

HeartbeatPacket make_heartbeat_packet() {
    HeartbeatPacket hb;
    hb.packet_type = PKT_HEARTBEAT;
    hb.timestamp_ms = host_to_be64(static_cast<uint64_t>(epoch_ms_now()));
    return hb;
}

void set_autopilot_fd(int fd) {
    std::lock_guard<std::mutex> lock(g_autopilot_fd_mutex);
    if (g_autopilot_fd.load() >= 0 && g_autopilot_fd.load() != fd) {
        g_network.tcp().close_fd(g_autopilot_fd.load());
    }
    g_autopilot_fd.store(fd);
}

void clear_autopilot_fd_if(int fd) {
    std::lock_guard<std::mutex> lock(g_autopilot_fd_mutex);
    if (g_autopilot_fd.load() == fd) {
        g_autopilot_fd.store(-1);
    }
}

void tcp_client_thread(int client_fd, std::string client_ip) {
    while (g_running.load()) {
        uint8_t packet_type = 0;
        if (!g_network.tcp().recv_all(client_fd, &packet_type, 1)) {
            break;
        }

        if (packet_type == PKT_HEARTBEAT) {
            HeartbeatPacket hb;
            hb.packet_type = packet_type;
            if (!g_network.tcp().recv_all(client_fd,
                                          reinterpret_cast<uint8_t*>(&hb) + 1,
                                          sizeof(HeartbeatPacket) - 1)) {
                break;
            }
            set_autopilot_fd(client_fd);
            if (g_send_heartbeat.load()) {
                HeartbeatPacket reply = make_heartbeat_packet();
                std::lock_guard<std::mutex> lock(g_autopilot_fd_mutex);
                g_network.tcp().send_all(client_fd, &reply, sizeof(reply));
            }
        } else if (packet_type == PKT_FLIGHT_CMD) {
            FlightModeCommand cmd;
            cmd.packet_type = packet_type;
            if (!g_network.tcp().recv_all(client_fd,
                                          reinterpret_cast<uint8_t*>(&cmd) + 1,
                                          sizeof(FlightModeCommand) - 1)) {
                break;
            }
            uint32_t seq = ntohl(cmd.sequence_id);
            std::cerr << "Flight command received seq=" << seq
                      << " mode=" << static_cast<int>(cmd.mode) << std::endl;

            CommandAck ack;
            ack.packet_type = PKT_CMD_ACK;
            ack.sequence_id = htonl(seq);
            ack.status = 0;
            g_network.tcp().send_all(client_fd, &ack, sizeof(ack));
        } else {
            std::cerr << "Unknown packet type " << static_cast<int>(packet_type)
                      << " from " << client_ip << std::endl;
            break;
        }
    }

    clear_autopilot_fd_if(client_fd);
    g_network.tcp().close_fd(client_fd);
}

void tcp_accept_loop() {
    if (!g_network.tcp().bind_and_listen(g_tcp_port)) {
        g_running.store(false);
        return;
    }

    while (g_running.load()) {
        std::string client_ip;
        int client_fd = g_network.tcp().accept_connection(client_ip);
        if (client_fd < 0) {
            if (!g_running.load()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(kAcceptPollMs));
            continue;
        }

        if (!is_whitelisted(client_ip)) {
            std::cerr << "[REJECT] IP=" << client_ip
                      << " not in subnet " << g_subnet_cidr << std::endl;
            g_network.tcp().close_fd(client_fd);
            continue;
        }

        std::thread(tcp_client_thread, client_fd, client_ip).detach();
    }
}

void heartbeat_sender_loop() {
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kHeartbeatIntervalMs));
        if (!g_send_heartbeat.load()) {
            continue;
        }

        int fd = g_autopilot_fd.load();
        if (fd < 0) {
            continue;
        }

        HeartbeatPacket hb = make_heartbeat_packet();
        std::lock_guard<std::mutex> lock(g_autopilot_fd_mutex);
        if (g_autopilot_fd.load() == fd) {
            if (!g_network.tcp().send_all(fd, &hb, sizeof(hb))) {
                clear_autopilot_fd_if(fd);
            }
        }
    }
}

bool validate_telemetry(TelemetryPacket& pkt) {
    uint8_t computed = NetworkLayer::compute_xor_checksum(
        reinterpret_cast<const uint8_t*>(&pkt),
        sizeof(TelemetryPacket) - 1);
    return computed == pkt.checksum;
}

void udp_recv_loop() {
    if (!g_network.udp().bind_port(g_udp_port)) {
        g_running.store(false);
        return;
    }

    uint8_t buf[sizeof(TelemetryPacket)];
    while (g_running.load()) {
        std::string sender_ip;
        ssize_t n = g_network.udp().recv_from(buf, sizeof(buf), sender_ip);
        if (n <= 0) {
            continue;
        }
        if (static_cast<size_t>(n) < sizeof(TelemetryPacket)) {
            continue;
        }

        TelemetryPacket pkt;
        std::memcpy(&pkt, buf, sizeof(pkt));
        if (pkt.packet_type != PKT_TELEMETRY) {
            continue;
        }

        bool valid = validate_telemetry(pkt);
        g_gcs_logic.on_telemetry_received(pkt, valid);
    }
}

void stats_writer_loop() {
    while (g_running.load()) {
        g_gcs_logic.write_stats_to_file();
        std::this_thread::sleep_for(std::chrono::milliseconds(kStatsIntervalMs));
    }
}

std::string read_line(TcpSocket& tcp, int fd) {
    std::string line;
    char c = 0;
    while (g_running.load()) {
        if (!tcp.recv_all(fd, &c, 1)) {
            return line;
        }
        if (c == '\n') {
            break;
        }
        line.push_back(c);
    }
    return line;
}

void handle_control_client(TcpSocket& tcp, int client_fd) {
    std::string line = read_line(tcp, client_fd);
    if (line == "GET corrupted") {
        std::ostringstream oss;
        oss << "corrupted_packet_count=" << g_gcs_logic.get_corrupted_count()
            << "\n";
        std::string resp = oss.str();
        tcp.send_all(client_fd, resp.data(), resp.size());
    } else if (line == "STOP_HEARTBEAT") {
        g_send_heartbeat.store(false);
        const char* ok = "OK\n";
        tcp.send_all(client_fd, ok, std::strlen(ok));
    } else if (line == "RESUME_HEARTBEAT") {
        g_send_heartbeat.store(true);
        const char* ok = "OK\n";
        tcp.send_all(client_fd, ok, std::strlen(ok));
    }
    tcp.close_fd(client_fd);
}

void control_port_loop() {
    TcpSocket ctrl_tcp;
    if (!ctrl_tcp.bind_and_listen(g_ctrl_port)) {
        g_running.store(false);
        return;
    }

    while (g_running.load()) {
        std::string client_ip;
        int client_fd = ctrl_tcp.accept_connection(client_ip);
        if (client_fd < 0) {
            if (!g_running.load()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(kAcceptPollMs));
            continue;
        }
        handle_control_client(ctrl_tcp, client_fd);
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    parse_args(argc, argv);
    std::signal(SIGINT, signal_handler);

    std::thread accept_thread(tcp_accept_loop);
    std::thread hb_thread(heartbeat_sender_loop);
    std::thread udp_thread(udp_recv_loop);
    std::thread stats_thread(stats_writer_loop);
    std::thread ctrl_thread(control_port_loop);

    accept_thread.join();
    hb_thread.join();
    udp_thread.join();
    stats_thread.join();
    ctrl_thread.join();

    return 0;
}
