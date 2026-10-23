// The platform's UDP sockets (POSIX) over the loopback interface: the behaviour
// UdpTransport relies on for FCL-UDP-002, FCL-UDP-003, FCL-UDP-005 and FCL-UDP-006.

#include "FprimeCsp/Udp/UdpTransport.hpp"

#include "TestSupport.hpp"

#include <sys/resource.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <vector>

namespace {

constexpr std::uint32_t LOOPBACK = 0x7F000001U;  // 127.0.0.1

/** Open a socket on the first free port from 40000 on; the port is returned in port. */
int openFree(FprimeCsp::UdpSocketApi& sockets, std::uint16_t& port, std::uint32_t timeoutMs = 200) {
    for (port = 40000; port < 40100; ++port) {
        const int socket = sockets.open(port, timeoutMs);
        if (socket >= 0) {
            return socket;
        }
    }
    return -1;
}

void udpSocketDatagramArrivesWithItsSender() {
    auto& sockets = FprimeCsp::platformUdpSockets();
    std::uint16_t portA = 0;
    std::uint16_t portB = 0;
    const int a = openFree(sockets, portA);
    const int b = openFree(sockets, portB);
    EXPECT_TRUE((a >= 0) && (b >= 0));

    const std::vector<std::uint8_t> data = {0xC5, 0x50, 0x00, 0x2A, 0x10, 0x41, 'h', 'i'};
    EXPECT_EQ(sockets.send(a, data.data(), data.size(), LOOPBACK, portB), static_cast<int>(data.size()));
    std::vector<std::uint8_t> buffer(64);
    std::uint32_t sender = 0;
    EXPECT_EQ(sockets.receive(b, buffer.data(), buffer.size(), sender), static_cast<int>(data.size()));
    buffer.resize(data.size());
    EXPECT_TRUE(buffer == data);
    EXPECT_EQ(sender, LOOPBACK);
    EXPECT_EQ(sockets.close(a), 0);
    EXPECT_EQ(sockets.close(b), 0);
}

void udpSocketReceiveWaitsAtMostTheTimeout() {
    auto& sockets = FprimeCsp::platformUdpSockets();
    std::uint16_t port = 0;
    const int socket = openFree(sockets, port, 100);
    EXPECT_TRUE(socket >= 0);

    std::uint8_t buffer[16];
    std::uint32_t sender = 0;
    const auto started = std::chrono::steady_clock::now();
    EXPECT_EQ(sockets.receive(socket, buffer, sizeof(buffer), sender), -EAGAIN);
    const auto waited = std::chrono::steady_clock::now() - started;
    EXPECT_TRUE(waited >= std::chrono::milliseconds(80));
    EXPECT_TRUE(waited < std::chrono::milliseconds(1000));
    EXPECT_EQ(sockets.close(socket), 0);
}

void udpSocketBoundPortCannotBeOpenedTwice() {
    auto& sockets = FprimeCsp::platformUdpSockets();
    std::uint16_t port = 0;
    const int socket = openFree(sockets, port);
    EXPECT_TRUE(socket >= 0);
    EXPECT_EQ(sockets.open(port, 200), -EADDRINUSE);
    EXPECT_EQ(sockets.close(socket), 0);
}

void udpSocketLongerDatagramIsCutToTheBuffer() {
    auto& sockets = FprimeCsp::platformUdpSockets();
    std::uint16_t portA = 0;
    std::uint16_t portB = 0;
    const int a = openFree(sockets, portA);
    const int b = openFree(sockets, portB);

    const std::vector<std::uint8_t> data(300, 0x5A);
    EXPECT_EQ(sockets.send(a, data.data(), data.size(), LOOPBACK, portB), 300);
    std::vector<std::uint8_t> buffer(100);
    std::uint32_t sender = 0;
    EXPECT_EQ(sockets.receive(b, buffer.data(), buffer.size(), sender), 100);
    EXPECT_EQ(sockets.close(a), 0);
    EXPECT_EQ(sockets.close(b), 0);
}

void udpSocketCloseReportsAnInvalidSocket() {
    auto& sockets = FprimeCsp::platformUdpSockets();
    std::uint16_t port = 0;
    const int socket = openFree(sockets, port);
    EXPECT_EQ(sockets.close(socket), 0);
    EXPECT_EQ(sockets.close(socket), -EBADF);
}

void udpSocketSendOnAnInvalidSocketFails() {
    auto& sockets = FprimeCsp::platformUdpSockets();
    const std::uint8_t byte = 0;
    EXPECT_EQ(sockets.send(-1, &byte, 1, LOOPBACK, 40000), -EBADF);
    std::uint32_t sender = 0;
    std::uint8_t buffer = 0;
    EXPECT_EQ(sockets.receive(-1, &buffer, 1, sender), -EBADF);
}

void udpSocketOpenFailureReportsTheErrno() {
    // With every file descriptor in use, socket() itself fails (EMFILE)
    rlimit original{};
    EXPECT_EQ(getrlimit(RLIMIT_NOFILE, &original), 0);
    rlimit low = original;
    low.rlim_cur = 64;
    EXPECT_EQ(setrlimit(RLIMIT_NOFILE, &low), 0);
    std::vector<int> taken;
    for (int fd = dup(0); fd >= 0; fd = dup(0)) {
        taken.push_back(fd);
    }
    std::uint16_t port = 40000;
    const int result = FprimeCsp::platformUdpSockets().open(port, 200);
    for (const int fd : taken) {
        (void)close(fd);
    }
    EXPECT_EQ(setrlimit(RLIMIT_NOFILE, &original), 0);
    EXPECT_EQ(result, -EMFILE);
}

}  // namespace

int main(int argc, char** argv) {
    return TestSupport::runSelected(
        argc, argv,
        {
            {"udp_socket_datagram_arrives_with_its_sender", udpSocketDatagramArrivesWithItsSender},
            {"udp_socket_receive_waits_at_most_the_timeout", udpSocketReceiveWaitsAtMostTheTimeout},
            {"udp_socket_bound_port_cannot_be_opened_twice", udpSocketBoundPortCannotBeOpenedTwice},
            {"udp_socket_longer_datagram_is_cut_to_the_buffer", udpSocketLongerDatagramIsCutToTheBuffer},
            {"udp_socket_close_reports_an_invalid_socket", udpSocketCloseReportsAnInvalidSocket},
            {"udp_socket_send_on_an_invalid_socket_fails", udpSocketSendOnAnInvalidSocketFails},
            {"udp_socket_open_failure_reports_the_errno", udpSocketOpenFailureReportsTheErrno},
        });
}
