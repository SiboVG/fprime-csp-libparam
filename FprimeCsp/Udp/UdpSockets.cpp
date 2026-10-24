// The platform's UDP sockets for UdpTransport: POSIX sockets on Linux and macOS,
// Zephyr's BSD sockets (zsock_*, CONFIG_NET_SOCKETS and CONFIG_NET_CONTEXT_RCVTIMEO) on Zephyr.

#include "FprimeCsp/Udp/UdpTransport.hpp"

#include <cerrno>

#if defined(__ZEPHYR__)
#include <zephyr/net/socket.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace FprimeCsp {

namespace {

#if defined(__ZEPHYR__)
using SockaddrIn = struct net_sockaddr_in;
using Sockaddr = struct net_sockaddr;
using SockLen = net_socklen_t;
using Timeval = struct zsock_timeval;
constexpr int FAMILY_INET = NET_AF_INET;
constexpr int TYPE_DGRAM = NET_SOCK_DGRAM;
constexpr int PROTO_UDP = NET_IPPROTO_UDP;
constexpr int LEVEL_SOCKET = ZSOCK_SOL_SOCKET;
constexpr int OPTION_RCVTIMEO = ZSOCK_SO_RCVTIMEO;
#define FPRIMECSP_SOCKET zsock_socket
#define FPRIMECSP_SETSOCKOPT zsock_setsockopt
#define FPRIMECSP_BIND zsock_bind
#define FPRIMECSP_SENDTO zsock_sendto
#define FPRIMECSP_RECVFROM zsock_recvfrom
#define FPRIMECSP_CLOSE zsock_close
#define FPRIMECSP_HTONS net_htons
#define FPRIMECSP_HTONL net_htonl
#define FPRIMECSP_NTOHL net_ntohl
#else
using SockaddrIn = struct sockaddr_in;
using Sockaddr = struct sockaddr;
using SockLen = socklen_t;
using Timeval = struct timeval;
constexpr int FAMILY_INET = AF_INET;
constexpr int TYPE_DGRAM = SOCK_DGRAM;
constexpr int PROTO_UDP = IPPROTO_UDP;
constexpr int LEVEL_SOCKET = SOL_SOCKET;
constexpr int OPTION_RCVTIMEO = SO_RCVTIMEO;
#define FPRIMECSP_SOCKET ::socket
#define FPRIMECSP_SETSOCKOPT ::setsockopt
#define FPRIMECSP_BIND ::bind
#define FPRIMECSP_SENDTO ::sendto
#define FPRIMECSP_RECVFROM ::recvfrom
#define FPRIMECSP_CLOSE ::close
#define FPRIMECSP_HTONS htons
#define FPRIMECSP_HTONL htonl
#define FPRIMECSP_NTOHL ntohl
#endif

SockaddrIn ipv4Address(const std::uint32_t address, const std::uint16_t port) {
    SockaddrIn result{};
    result.sin_family = FAMILY_INET;
    result.sin_port = FPRIMECSP_HTONS(port);
    result.sin_addr.s_addr = FPRIMECSP_HTONL(address);
    return result;
}

/** errno as a negative value; a timeout or an interrupted wait is -EAGAIN. */
int lastError() {
    const int error = errno;
    if ((error == EAGAIN) || (error == EWOULDBLOCK) || (error == EINTR)) {
        return -EAGAIN;
    }
    return -error;
}

class PlatformUdpSockets final : public UdpSocketApi {
  public:
    int open(const std::uint16_t port, const std::uint32_t timeoutMs) override {
        const int socket = FPRIMECSP_SOCKET(FAMILY_INET, TYPE_DGRAM, PROTO_UDP);
        if (socket < 0) {
            return -errno;
        }
        Timeval timeout{};
        timeout.tv_sec = static_cast<decltype(timeout.tv_sec)>(timeoutMs / 1000U);
        timeout.tv_usec = static_cast<decltype(timeout.tv_usec)>((timeoutMs % 1000U) * 1000U);
        const SockaddrIn local = ipv4Address(0U, port);
        if ((FPRIMECSP_SETSOCKOPT(socket, LEVEL_SOCKET, OPTION_RCVTIMEO, &timeout, sizeof(timeout)) != 0) ||
            (FPRIMECSP_BIND(socket, reinterpret_cast<const Sockaddr*>(&local), sizeof(local)) != 0)) {
            const int error = -errno;
            (void)FPRIMECSP_CLOSE(socket);
            return error;
        }
        return socket;
    }

    int send(const int socket,
             const std::uint8_t* data,
             const std::size_t size,
             const std::uint32_t address,
             const std::uint16_t port) override {
        const SockaddrIn peer = ipv4Address(address, port);
        const auto sent =
            FPRIMECSP_SENDTO(socket, data, size, 0, reinterpret_cast<const Sockaddr*>(&peer), sizeof(peer));
        return (sent < 0) ? -errno : static_cast<int>(sent);
    }

    int receive(const int socket, std::uint8_t* data, const std::size_t capacity, std::uint32_t& sender) override {
        SockaddrIn from{};
        SockLen fromSize = sizeof(from);
        const auto received =
            FPRIMECSP_RECVFROM(socket, data, capacity, 0, reinterpret_cast<Sockaddr*>(&from), &fromSize);
        if (received < 0) {
            return lastError();
        }
        sender = FPRIMECSP_NTOHL(from.sin_addr.s_addr);
        return static_cast<int>(received);
    }

    int close(const int socket) override {
        return (FPRIMECSP_CLOSE(socket) != 0) ? -errno : 0;
    }
};

}  // namespace

UdpSocketApi& platformUdpSockets() {
    static PlatformUdpSockets sockets;
    return sockets;
}

}  // namespace FprimeCsp
