#include "network_layer/TcpSocket.h"
#include "platform_posix.h"

#include <cstring>
#include <iostream>

TcpSocket::TcpSocket() : server_fd_(-1) {}

TcpSocket::~TcpSocket() {
    if (server_fd_ >= 0) {
        close_fd(server_fd_);
        server_fd_ = -1;
    }
}

bool TcpSocket::bind_and_listen(uint16_t port, int backlog) {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "socket: " << std::strerror(errno) << std::endl;
        return false;
    }

    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "setsockopt SO_REUSEADDR: " << std::strerror(errno) << std::endl;
        close_fd(server_fd_);
        server_fd_ = -1;
        return false;
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "bind: " << std::strerror(errno) << std::endl;
        close_fd(server_fd_);
        server_fd_ = -1;
        return false;
    }

    if (listen(server_fd_, backlog) < 0) {
        std::cerr << "listen: " << std::strerror(errno) << std::endl;
        close_fd(server_fd_);
        server_fd_ = -1;
        return false;
    }

    return true;
}

int TcpSocket::accept_connection(std::string& out_client_ip) {
    if (server_fd_ < 0) {
        return -1;
    }

    sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    int client_fd = accept(server_fd_, reinterpret_cast<sockaddr*>(&client_addr), &len);
    if (client_fd < 0) {
        if (errno != EINTR) {
            std::cerr << "accept: " << std::strerror(errno) << std::endl;
        }
        return -1;
    }

    char ip_buf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &client_addr.sin_addr, ip_buf, sizeof(ip_buf)) == NULL) {
        std::cerr << "inet_ntop: " << std::strerror(errno) << std::endl;
        out_client_ip = "";
    } else {
        out_client_ip = ip_buf;
    }

    return client_fd;
}

bool TcpSocket::connect_to(const std::string& host, uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cerr << "socket: " << std::strerror(errno) << std::endl;
        return false;
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "inet_pton: " << std::strerror(errno) << std::endl;
        close_fd(fd);
        return false;
    }

    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "connect: " << std::strerror(errno) << std::endl;
        close_fd(fd);
        return false;
    }

    server_fd_ = fd;
    return true;
}

bool TcpSocket::send_all(int fd, const void* buf, size_t len) {
    const uint8_t* ptr = static_cast<const uint8_t*>(buf);
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, ptr + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "send: " << std::strerror(errno) << std::endl;
            return false;
        }
        if (n == 0) {
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool TcpSocket::recv_all(int fd, void* buf, size_t len) {
    uint8_t* ptr = static_cast<uint8_t*>(buf);
    size_t received = 0;

    while (received < len) {
        ssize_t n = recv(fd, ptr + received, len - received, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "recv: " << std::strerror(errno) << std::endl;
            return false;
        }
        if (n == 0) {
            return false;
        }
        received += static_cast<size_t>(n);
    }
    return true;
}

void TcpSocket::close_fd(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

int TcpSocket::get_server_fd() const {
    return server_fd_;
}
