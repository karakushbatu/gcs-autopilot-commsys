#ifndef UDP_SOCKET_H
#define UDP_SOCKET_H

#include <cstddef>
#include <cstdint>
#include <string>

class UdpSocket {
public:
    UdpSocket();
    ~UdpSocket();

    bool ensure_open();
    bool bind_port(uint16_t port);
    bool send_to(const std::string& host, uint16_t port,
                 const void* buf, size_t len);
    ssize_t recv_from(void* buf, size_t max_len, std::string& out_sender_ip);
    void close_socket();

private:
    int sock_fd_;
};

#endif
