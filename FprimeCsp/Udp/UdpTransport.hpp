#ifndef FPRIMECSP_UDP_UDPTRANSPORT_HPP
#define FPRIMECSP_UDP_UDPTRANSPORT_HPP

#include "FprimeCsp/Runtime/Stack.hpp"

#include <Os/Task.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

extern "C" {
#include <csp/csp_interface.h>
}

namespace FprimeCsp {

/**
 * Point-to-point CSP over UDP settings. The wire format is that of libcsp
 * csh/2.10's csp_if_udp (csh `csp add udp`, default port 9220): one datagram per
 * CSP packet, the CSP header followed by the packet data.
 */
struct UdpConfig {
    /** The peer's unicast IPv4 address, dotted decimal; required. */
    std::string peer;
    /** UDP port this transport receives on. */
    std::uint16_t localPort = 9220;
    /** UDP port of the peer. */
    std::uint16_t remotePort = 9220;
    /** Name of the CSP interface; at most CSP_IFLIST_NAME_MAX characters, unique. */
    std::string interfaceName = "UDP";
    bool defaultRoute = true;
    /** Priority and stack size of the UdpReceive task; the defaults leave them to the OS layer. */
    TaskPriority receivePriority = Os::Task::TASK_PRIORITY_DEFAULT;
    TaskSize receiveStackSize = Os::Task::TASK_DEFAULT;
};

/** The UDP socket operations the transport uses: the platform's sockets, or a test double. */
class UdpSocketApi {
  public:
    virtual ~UdpSocketApi() = default;

    /**
     * Open a UDP socket bound to port on every local address, whose receive waits
     * at most timeoutMs. Returns the socket (>= 0) or a negative errno value.
     */
    virtual int open(std::uint16_t port, std::uint32_t timeoutMs) = 0;
    /** Send one datagram to address (IPv4, host byte order) and port. Returns the bytes sent or a negative errno value. */
    virtual int send(int socket, const std::uint8_t* data, std::size_t size, std::uint32_t address, std::uint16_t port) = 0;
    /**
     * Receive one datagram into data, at most capacity bytes, and its sender's address
     * (IPv4, host byte order). Returns its length, -EAGAIN when the timeout passed
     * without one, or another negative errno value.
     */
    virtual int receive(int socket, std::uint8_t* data, std::size_t capacity, std::uint32_t& sender) = 0;
    /** Close the socket. Returns 0 or a negative errno value. */
    virtual int close(int socket) = 0;
};

/** The platform's UDP sockets: POSIX on Linux, Zephyr's BSD sockets on Zephyr. */
UdpSocketApi& platformUdpSockets();

/**
 * CSP over UDP to one peer, process-final like the other transports: open() registers a
 * CSP interface and starts the UdpReceive task; close() unregisters the interface, stops
 * the task and closes the socket, and can be repeated after a failed phase.
 */
class UdpTransport final : public Transport {
  public:
    /** The receive timeout, which bounds how long close() waits for the UdpReceive task. */
    static constexpr std::uint32_t RECEIVE_TIMEOUT_MS = 200;

    explicit UdpTransport(UdpConfig config, UdpSocketApi& sockets = platformUdpSockets());
    ~UdpTransport() override;

    UdpTransport(const UdpTransport&) = delete;
    UdpTransport& operator=(const UdpTransport&) = delete;

    /**
     * CSP_ERR_NONE if open() accepts the configuration, otherwise CSP_ERR_INVAL: a peer that
     * is not a unicast IPv4 address in dotted decimal, a zero port, or an interface name that
     * is empty, too long for libcsp, or already registered with libcsp.
     */
    static int validate(const UdpConfig& config);

    bool isOpen() const;
    csp_iface_t* interface();
    /** The errno value of the last failed socket operation, 0 if none failed. */
    int socketError() const;

  private:
    int open(std::uint16_t localNode, std::uint8_t networkPrefix) override;
    int close() override;

    static int nexthop(csp_iface_t* iface, std::uint16_t via, csp_packet_t* packet, int fromMe);
    static void receiveEntry(void* context);
    void receiveLoop();
    void deliver(const std::uint8_t* datagram, std::size_t size);

    UdpConfig m_config;
    UdpSocketApi& m_sockets;
    std::uint32_t m_peer = 0;
    int m_socket = -1;
    std::atomic<int> m_socketError{0};
    csp_iface_t m_interface{};
    bool m_registered = false;
    Os::Task m_receiveTask;
    bool m_receiveStarted = false;
    std::atomic<bool> m_receiving{false};
};

}  // namespace FprimeCsp

#endif  // FPRIMECSP_UDP_UDPTRANSPORT_HPP
