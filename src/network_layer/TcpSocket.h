#ifndef TCP_SOCKET_H
#define TCP_SOCKET_H

#include <cstddef>
#include <cstdint>

#include <string>

class TcpSocket {
public:
    TcpSocket();
    ~TcpSocket();

    bool bind_and_listen(uint16_t port, int backlog = 10);
    int accept_connection(std::string& out_client_ip);

    bool connect_to(const std::string& host, uint16_t port);

    bool send_all(int fd, const void* buf, size_t len);
    bool recv_all(int fd, void* buf, size_t len);
    void close_fd(int fd);
    int get_server_fd() const;

private:
    int server_fd_;
};

#endif
