#ifndef FLIGHT_LOGIC_H
#define FLIGHT_LOGIC_H

#include "network_layer/NetworkLayer.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

enum class AutopilotState { IDLE, FLYING, SAFE_MODE };

class Autopilot {
public:
    Autopilot();

    void set_state(AutopilotState s);
    AutopilotState get_state() const;

    std::atomic<int64_t> last_heartbeat_ms;
    std::atomic<bool> running;

private:
    mutable std::mutex state_mutex_;
    AutopilotState current_state_;
};

class GCSLogic {
public:
    GCSLogic();

    void on_telemetry_received(const TelemetryPacket& pkt, bool valid);
    int get_corrupted_count() const;
    void write_stats_to_file();

    TelemetryPacket latest_telemetry;
    std::mutex telemetry_mutex;
    int64_t latest_telemetry_recv_ms;

    std::atomic<int> corrupted_packet_count;
};

#endif
