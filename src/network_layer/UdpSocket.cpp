#include "network_layer/UdpSocket.h"
#include "platform_posix.h"

#include <cstring>
#include <iostream>

namespace {
const int kSelectTimeoutMs = 5;
}

UdpSocket::UdpSocket() : sock_fd_(-1) {}

UdpSocket::~UdpSocket() {
    close_socket();
}

bool UdpSocket::ensure_open() {
    if (sock_fd_ >= 0) {
        return true;
    }
    sock_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd_ < 0) {
        std::cerr << "socket: " << std::strerror(errno) << std::endl;
        return false;
    }
    return true;
}

bool UdpSocket::bind_port(uint16_t port) {
    sock_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd_ < 0) {
        std::cerr << "socket: " << std::strerror(errno) << std::endl;
        return false;
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "bind: " << std::strerror(errno) << std::endl;
        close_socket();
        return false;
    }

    return true;
}

bool UdpSocket::send_to(const std::string& host, uint16_t port,
                        const void* buf, size_t len) {
    if (!ensure_open()) {
        return false;
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "inet_pton: " << std::strerror(errno) << std::endl;
        return false;
    }

    ssize_t n = sendto(sock_fd_, buf, len, 0,
                       reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (n < 0) {
        std::cerr << "sendto: " << std::strerror(errno) << std::endl;
        return false;
    }
    if (static_cast<size_t>(n) != len) {
        std::cerr << "sendto: short write" << std::endl;
        return false;
    }
    return true;
}

ssize_t UdpSocket::recv_from(void* buf, size_t max_len, std::string& out_sender_ip) {
    if (sock_fd_ < 0) {
        return -1;
    }

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock_fd_, &readfds);

    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = kSelectTimeoutMs * 1000;

    int ready = select(sock_fd_ + 1, &readfds, NULL, NULL, &tv);
    if (ready < 0) {
        if (errno != EINTR) {
            std::cerr << "select: " << std::strerror(errno) << std::endl;
        }
        return -1;
    }
    if (ready == 0) {
        return 0;
    }

    sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);
    ssize_t n = recvfrom(sock_fd_, buf, max_len, 0,
                         reinterpret_cast<sockaddr*>(&sender), &sender_len);
    if (n < 0) {
        if (errno != EINTR) {
            std::cerr << "recvfrom: " << std::strerror(errno) << std::endl;
        }
        return -1;
    }

    char ip_buf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &sender.sin_addr, ip_buf, sizeof(ip_buf)) == NULL) {
        std::cerr << "inet_ntop: " << std::strerror(errno) << std::endl;
        out_sender_ip = "";
    } else {
        out_sender_ip = ip_buf;
    }

    return n;
}

void UdpSocket::close_socket() {
    if (sock_fd_ >= 0) {
        close(sock_fd_);
        sock_fd_ = -1;
    }
}
