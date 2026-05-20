#include "flight_logic/FlightLogic.h"
#include "platform_posix.h"

#include <chrono>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
const char* kStatsFilePath = "/tmp/gcs_stats.txt";

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

const char* state_name(AutopilotState s) {
    switch (s) {
    case AutopilotState::IDLE:
        return "IDLE";
    case AutopilotState::FLYING:
        return "FLYING";
    case AutopilotState::SAFE_MODE:
        return "SAFE_MODE";
    default:
        return "UNKNOWN";
    }
}
}

Autopilot::Autopilot()
    : last_heartbeat_ms(epoch_ms_now()),
      running(true),
      current_state_(AutopilotState::IDLE) {}

void Autopilot::set_state(AutopilotState s) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (current_state_ == s) {
        return;
    }
    current_state_ = s;
    std::cerr << "[" << iso_timestamp() << "] Autopilot state -> "
              << state_name(s) << std::endl;
}

AutopilotState Autopilot::get_state() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return current_state_;
}

GCSLogic::GCSLogic()
    : latest_telemetry_recv_ms(0),
      corrupted_packet_count(0) {
    std::memset(&latest_telemetry, 0, sizeof(latest_telemetry));
}

void GCSLogic::on_telemetry_received(const TelemetryPacket& pkt, bool valid) {
    if (valid) {
        std::lock_guard<std::mutex> lock(telemetry_mutex);
        latest_telemetry = pkt;
        latest_telemetry_recv_ms = epoch_ms_now();
    } else {
        corrupted_packet_count.fetch_add(1);
        std::cerr << "WARNING: corrupted telemetry packet dropped" << std::endl;
    }
}

int GCSLogic::get_corrupted_count() const {
    return corrupted_packet_count.load();
}

void GCSLogic::write_stats_to_file() {
    std::ofstream out(kStatsFilePath, std::ios::trunc);
    if (!out) {
        std::cerr << "failed to open stats file: " << kStatsFilePath << std::endl;
        return;
    }
    out << "corrupted_packet_count=" << get_corrupted_count() << "\n";
}
