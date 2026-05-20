#ifndef PLATFORM_POSIX_H
#define PLATFORM_POSIX_H

#include <cstdint>
#include <ctime>

#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifndef ssize_t
typedef int ssize_t;
#endif
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

inline uint64_t host_to_be64(uint64_t value) {
    const uint32_t high =
        htonl(static_cast<uint32_t>(value >> 32));
    const uint32_t low =
        htonl(static_cast<uint32_t>(value & 0xFFFFFFFFULL));
    return (static_cast<uint64_t>(low) << 32) |
           static_cast<uint64_t>(high);
}

inline void platform_localtime(std::time_t seconds, std::tm* out) {
#if defined(_WIN32)
    localtime_s(out, &seconds);
#else
    localtime_r(&seconds, out);
#endif
}

#endif
