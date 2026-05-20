#include "network_layer/NetworkLayer.h"
#include "platform_posix.h"

#include <cstring>
#include <iostream>

NetworkLayer::NetworkLayer() {}

TcpSocket& NetworkLayer::tcp() {
    return tcp_;
}

UdpSocket& NetworkLayer::udp() {
    return udp_;
}

uint8_t NetworkLayer::compute_xor_checksum(const uint8_t* data, size_t len) {
    uint8_t checksum = 0;
    for (size_t i = 0; i < len; ++i) {
        checksum ^= data[i];
    }
    return checksum;
}

bool NetworkLayer::is_ip_in_subnet(const std::string& ip,
                                   const std::string& subnet_ip,
                                   const std::string& subnet_mask) {
    in_addr ip_addr;
    in_addr net_addr;
    in_addr mask_addr;

    if (inet_pton(AF_INET, ip.c_str(), &ip_addr) != 1) {
        std::cerr << "inet_pton ip: " << std::strerror(errno) << std::endl;
        return false;
    }
    if (inet_pton(AF_INET, subnet_ip.c_str(), &net_addr) != 1) {
        std::cerr << "inet_pton subnet: " << std::strerror(errno) << std::endl;
        return false;
    }
    if (inet_pton(AF_INET, subnet_mask.c_str(), &mask_addr) != 1) {
        std::cerr << "inet_pton mask: " << std::strerror(errno) << std::endl;
        return false;
    }

    uint32_t ip_host = ntohl(ip_addr.s_addr);
    uint32_t net_host = ntohl(net_addr.s_addr);
    uint32_t mask_host = ntohl(mask_addr.s_addr);

    return (ip_host & mask_host) == (net_host & mask_host);
}
