// CSP over UDP (FCL-UDP-001 to FCL-UDP-006), against fake libcsp functions and a fake
// socket layer. Expected values come from the requirements: the datagram is the CSP
// header (here the fake's 6 bytes, the size of a CSP v2 header) followed by the data;
// 192.168.70.1 is 0xC0A84601 in host byte order.

#include "FprimeCsp/Udp/UdpTransport.hpp"

#include "TestDoubles.hpp"
#include "TestSupport.hpp"

#include <cerrno>
#include <chrono>
#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

extern "C" {
#include <csp/csp_buffer.h>
#include <csp/csp_iflist.h>
}

namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr std::uint32_t PEER = 0xC0A84601U;       // 192.168.70.1
constexpr std::uint32_t STRANGER = 0xC0A84609U;   // 192.168.70.9
constexpr std::size_t HEADER = sizeof(TestDoubles::FAKE_HEADER);
constexpr int SOCKET = 7;

class FakeUdpSockets final : public FprimeCsp::UdpSocketApi {
  public:
    struct Sent {
        Bytes data;
        std::uint32_t address;
        std::uint16_t port;
    };

    int openStatus = SOCKET;
    int openCalls = 0;
    std::uint16_t openPort = 0;
    std::uint32_t openTimeoutMs = 0;
    int sendStatus = 0;  // 0: send the whole datagram; otherwise returned as is
    std::vector<Sent> sent;
    int closeStatus = 0;
    int closeCalls = 0;

    int open(std::uint16_t port, std::uint32_t timeoutMs) override {
        ++this->openCalls;
        this->openPort = port;
        this->openTimeoutMs = timeoutMs;
        return this->openStatus;
    }

    int send(int, const std::uint8_t* data, std::size_t size, std::uint32_t address, std::uint16_t port) override {
        this->sent.push_back({Bytes(data, data + size), address, port});
        return (this->sendStatus != 0) ? this->sendStatus : static_cast<int>(size);
    }

    int receive(int, std::uint8_t* data, std::size_t capacity, std::uint32_t& sender) override {
        std::unique_lock<std::mutex> lock(this->m_mutex);
        // As a socket with a receive timeout: wait at most the timeout open() was given
        if (!this->m_ready.wait_for(lock, std::chrono::milliseconds(this->openTimeoutMs),
                                    [this] { return !this->m_queue.empty(); })) {
            return -EAGAIN;
        }
        const Arrival datagram = this->m_queue.front();
        this->m_queue.pop_front();
        if (datagram.error != 0) {
            return -datagram.error;
        }
        // As recvfrom(): a longer datagram is cut to the buffer
        const std::size_t size = std::min(capacity, datagram.data.size());
        if (size > 0U) {
            std::memcpy(data, datagram.data.data(), size);
        }
        sender = datagram.sender;
        return static_cast<int>(size);
    }

    int close(int) override {
        ++this->closeCalls;
        TestDoubles::cspState().closeOrder.emplace_back("close");
        return this->closeStatus;
    }

    void arrive(const Bytes& datagram, std::uint32_t sender = PEER) {
        this->enqueue({datagram, sender, 0});
    }

    /** The next receive fails with this errno value. */
    void fail(int error) {
        this->enqueue({{}, 0, error});
    }

  private:
    struct Arrival {
        Bytes data;
        std::uint32_t sender;
        int error;
    };

    void enqueue(Arrival arrival) {
        {
            const std::lock_guard<std::mutex> lock(this->m_mutex);
            this->m_queue.push_back(std::move(arrival));
        }
        this->m_ready.notify_one();
    }

    std::mutex m_mutex;
    std::condition_variable m_ready;
    std::deque<Arrival> m_queue;
};

FprimeCsp::StackConfig stackConfig() {
    FprimeCsp::StackConfig config;
    config.localNode = 6061;
    config.networkPrefix = 8;
    return config;
}

FprimeCsp::UdpConfig udpConfig() {
    FprimeCsp::UdpConfig config;
    config.peer = "192.168.70.1";
    config.localPort = 9220;
    config.remotePort = 9221;
    return config;
}

void reset() {
    TestDoubles::resetCsp();
    Os::Test::resetTaskFake();
}

bool waitUntil(const std::function<bool()>& condition, int milliseconds = 2000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    while (!condition()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

/** A datagram with the given number of data bytes after the header: 0, 1, 2, ... */
Bytes datagram(std::size_t dataBytes) {
    Bytes frame(TestDoubles::FAKE_HEADER, TestDoubles::FAKE_HEADER + HEADER);
    for (std::size_t i = 0; i < dataBytes; ++i) {
        frame.push_back(static_cast<std::uint8_t>(i));
    }
    return frame;
}

/** Send a packet with these data bytes through the transport's CSP interface. */
int sendThrough(FprimeCsp::UdpTransport& transport, const Bytes& data, csp_packet_t** packetOut = nullptr) {
    csp_packet_t* packet = csp_buffer_get(0);
    if (!data.empty()) {
        std::memcpy(packet->data, data.data(), data.size());
    }
    packet->length = static_cast<std::uint16_t>(data.size());
    if (packetOut != nullptr) {
        *packetOut = packet;
    }
    csp_iface_t* iface = transport.interface();
    return iface->nexthop(iface, 0, packet, 1);
}

// --- FCL-UDP-001: one datagram per packet, header and data, nothing else -------------

void udp001SendIsHeaderAndDataOnly() {
    reset();
    FakeUdpSockets sockets;
    FprimeCsp::UdpTransport transport(udpConfig(), sockets);
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(stackConfig(), transport));

    const Bytes data = {'p', 'i', 'n', 'g'};
    EXPECT_EQ(sendThrough(transport, data), CSP_ERR_NONE);

    EXPECT_EQ(sockets.sent.size(), static_cast<std::size_t>(1));
    Bytes expected(TestDoubles::FAKE_HEADER, TestDoubles::FAKE_HEADER + HEADER);
    expected.insert(expected.end(), data.begin(), data.end());
    EXPECT_TRUE(sockets.sent[0].data == expected);
    stack.shutdown();
}

void udp001EmptyPacketIsTheHeaderAlone() {
    reset();
    FakeUdpSockets sockets;
    FprimeCsp::UdpTransport transport(udpConfig(), sockets);
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(stackConfig(), transport));

    EXPECT_EQ(sendThrough(transport, {}), CSP_ERR_NONE);
    EXPECT_EQ(sockets.sent[0].data.size(), HEADER);
    stack.shutdown();
}

void udp001ReceivedDatagramReachesTheRouter() {
    reset();
    FakeUdpSockets sockets;
    FprimeCsp::UdpTransport transport(udpConfig(), sockets);
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(stackConfig(), transport));

    sockets.arrive(datagram(3));
    EXPECT_TRUE(waitUntil([] { return TestDoubles::cspState().deliveredCount.load() == 1; }));
    const auto& delivered = TestDoubles::cspState().delivered[0];
    EXPECT_TRUE(delivered.frame == datagram(3));
    EXPECT_TRUE(delivered.data == Bytes({0, 1, 2}));
    EXPECT_TRUE(delivered.iface == transport.interface());
    stack.shutdown();
}

// --- FCL-UDP-002: invalid datagrams are dropped and counted ---------------------------

void expectRejected(const Bytes& frame, std::uint32_t sender) {
    reset();
    FakeUdpSockets sockets;
    FprimeCsp::UdpTransport transport(udpConfig(), sockets);
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(stackConfig(), transport));

    // The receive task handles datagrams in order: once the valid one that follows is
    // delivered, the rejected one has been handled (and the counters may be read)
    sockets.arrive(frame, sender);
    sockets.arrive(datagram(1));
    EXPECT_TRUE(waitUntil([] { return TestDoubles::cspState().deliveredCount.load() == 1; }));
    EXPECT_TRUE(TestDoubles::cspState().delivered[0].frame == datagram(1));
    EXPECT_EQ(transport.interface()->rx_error, 1U);
    EXPECT_EQ(transport.interface()->drop, 0U);
    stack.shutdown();
    EXPECT_EQ(TestDoubles::cspState().buffersInUse.load(), 0);
}

void udp002EmptyDatagramIsRejected() {
    expectRejected({}, PEER);
}

void udp002DatagramOneByteShorterThanTheHeaderIsRejected() {
    expectRejected(Bytes(TestDoubles::FAKE_HEADER, TestDoubles::FAKE_HEADER + HEADER - 1), PEER);
}

void udp002DatagramOneByteLongerThanTheLargestPacketIsRejected() {
    expectRejected(datagram(CSP_BUFFER_SIZE + 1), PEER);
}

void udp002MuchLongerDatagramIsRejected() {
    expectRejected(datagram(4 * CSP_BUFFER_SIZE), PEER);
}

void udp002DatagramFromAnotherSenderIsRejected() {
    expectRejected(datagram(3), STRANGER);
}

void udp002HeaderTheStackRefusesIsRejected() {
    reset();
    FakeUdpSockets sockets;
    FprimeCsp::UdpTransport transport(udpConfig(), sockets);
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(stackConfig(), transport));

    TestDoubles::cspState().stripFailures.store(1);
    sockets.arrive(datagram(3));
    sockets.arrive(datagram(1));
    EXPECT_TRUE(waitUntil([] { return TestDoubles::cspState().deliveredCount.load() == 1; }));
    EXPECT_TRUE(TestDoubles::cspState().delivered[0].frame == datagram(1));
    EXPECT_EQ(transport.interface()->rx_error, 1U);
    stack.shutdown();
    EXPECT_EQ(TestDoubles::cspState().buffersInUse.load(), 0);
}

void udp002BoundarySizesAreDelivered() {
    reset();
    FakeUdpSockets sockets;
    FprimeCsp::UdpTransport transport(udpConfig(), sockets);
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(stackConfig(), transport));

    sockets.arrive(datagram(0));                 // the header alone
    sockets.arrive(datagram(CSP_BUFFER_SIZE));   // the largest packet
    EXPECT_TRUE(waitUntil([] { return TestDoubles::cspState().deliveredCount.load() == 2; }));
    EXPECT_EQ(TestDoubles::cspState().delivered[0].data.size(), static_cast<std::size_t>(0));
    EXPECT_EQ(TestDoubles::cspState().delivered[1].data.size(), static_cast<std::size_t>(CSP_BUFFER_SIZE));
    EXPECT_EQ(transport.interface()->rx_error, 0U);
    stack.shutdown();
}

void udp002DatagramWithoutAFreeBufferIsDropped() {
    reset();
    FakeUdpSockets sockets;
    FprimeCsp::UdpTransport transport(udpConfig(), sockets);
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(stackConfig(), transport));

    TestDoubles::cspState().bufferGetFailures.store(1);
    sockets.arrive(datagram(3));
    sockets.arrive(datagram(2));
    EXPECT_TRUE(waitUntil([] { return TestDoubles::cspState().deliveredCount.load() == 1; }));
    EXPECT_TRUE(TestDoubles::cspState().delivered[0].frame == datagram(2));
    EXPECT_EQ(transport.interface()->drop, 1U);
    EXPECT_EQ(transport.interface()->rx_error, 0U);
    stack.shutdown();
}

void udp002ReceiveErrorDoesNotStopReceiving() {
    reset();
    FakeUdpSockets sockets;
    FprimeCsp::UdpTransport transport(udpConfig(), sockets);
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(stackConfig(), transport));

    // e.g. after an ICMP port unreachable for an earlier datagram to the peer
    sockets.fail(ECONNREFUSED);
    sockets.arrive(datagram(2));
    EXPECT_TRUE(waitUntil([] { return TestDoubles::cspState().deliveredCount.load() == 1; }));
    EXPECT_EQ(transport.socketError(), ECONNREFUSED);
    EXPECT_EQ(transport.interface()->rx_error, 0U);
    stack.shutdown();
}

// --- FCL-UDP-003: to the peer; the packet's owner on success and on failure -----------

void udp003SendGoesToThePeerAddressAndPort() {
    reset();
    FakeUdpSockets sockets;
    FprimeCsp::UdpTransport transport(udpConfig(), sockets);
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(stackConfig(), transport));

    EXPECT_EQ(sendThrough(transport, {1, 2}), CSP_ERR_NONE);
    EXPECT_EQ(sockets.sent[0].address, PEER);
    EXPECT_EQ(sockets.sent[0].port, static_cast<std::uint16_t>(9221));
    EXPECT_EQ(sockets.openPort, static_cast<std::uint16_t>(9220));
    stack.shutdown();
}

void udp003SentPacketIsReleasedOnce() {
    reset();
    FakeUdpSockets sockets;
    FprimeCsp::UdpTransport transport(udpConfig(), sockets);
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(stackConfig(), transport));

    const int freedBefore = TestDoubles::cspState().bufferFreeCalls.load();
    EXPECT_EQ(sendThrough(transport, {1, 2}), CSP_ERR_NONE);
    EXPECT_EQ(TestDoubles::cspState().bufferFreeCalls.load() - freedBefore, 1);
    EXPECT_EQ(TestDoubles::cspState().buffersInUse.load(), 0);
    stack.shutdown();
}

void expectSendFailureLeavesThePacket(int sendStatus) {
    reset();
    FakeUdpSockets sockets;
    sockets.sendStatus = sendStatus;
    FprimeCsp::UdpTransport transport(udpConfig(), sockets);
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(stackConfig(), transport));

    csp_packet_t* packet = nullptr;
    const int freedBefore = TestDoubles::cspState().bufferFreeCalls.load();
    EXPECT_TRUE(sendThrough(transport, {1, 2, 3, 4}, &packet) != CSP_ERR_NONE);
    // The packet is still the caller's: libcsp frees it and counts the error
    EXPECT_EQ(TestDoubles::cspState().bufferFreeCalls.load(), freedBefore);
    EXPECT_EQ(TestDoubles::cspState().buffersInUse.load(), 1);
    csp_buffer_free(packet);
    stack.shutdown();
}

void udp003SendErrorLeavesThePacketToLibcsp() {
    expectSendFailureLeavesThePacket(-EHOSTUNREACH);
}

void udp003PartialSendIsAFailure() {
    expectSendFailureLeavesThePacket(static_cast<int>(HEADER + 1));
}

// --- FCL-UDP-004: configuration refused before a socket or interface exists -----------

FprimeCsp::UdpConfig withPeer(const char* peer) {
    FprimeCsp::UdpConfig config = udpConfig();
    config.peer = peer;
    return config;
}

void expectInvalid(const FprimeCsp::UdpConfig& config) {
    EXPECT_EQ(FprimeCsp::UdpTransport::validate(config), CSP_ERR_INVAL);
}

void udp004PeerThatIsNotAnAddressIsInvalid() {
    reset();
    for (const char* peer : {"", "192.168.70", "192.168.70.1.5", "192.168.70.256", "192.168.70.1x",
                             " 192.168.70.1", "192.168.70.1 ", "192..70.1", "-1.2.3.4", "1.2.3.", ".1.2.3",
                             "1.2.3.0400", "1.2.3.4/24"}) {
        expectInvalid(withPeer(peer));
    }
}

void udp004PeerThatIsNotUnicastIsInvalid() {
    reset();
    for (const char* peer : {"0.0.0.0", "255.255.255.255", "224.0.0.1", "239.255.255.255", "240.0.0.1"}) {
        expectInvalid(withPeer(peer));
    }
}

void udp004ZeroPortsAreInvalid() {
    reset();
    FprimeCsp::UdpConfig config = udpConfig();
    config.localPort = 0;
    expectInvalid(config);
    config = udpConfig();
    config.remotePort = 0;
    expectInvalid(config);
}

void udp004InterfaceNamesOutOfBoundsAreInvalid() {
    reset();
    FprimeCsp::UdpConfig config = udpConfig();
    config.interfaceName.clear();
    expectInvalid(config);
    config.interfaceName = "UDP45678901";  // 11 characters, one more than CSP_IFLIST_NAME_MAX
    expectInvalid(config);
}

void udp004RegisteredInterfaceNameIsInvalid() {
    reset();
    csp_iface_t other{};
    other.name = "UDP";
    csp_iflist_add(&other);
    expectInvalid(udpConfig());
}

void udp004ValidBoundariesAreAccepted() {
    reset();
    for (const char* peer : {"1.0.0.1", "127.0.0.1", "192.168.70.1", "223.255.255.254", "10.0.0.255"}) {
        EXPECT_EQ(FprimeCsp::UdpTransport::validate(withPeer(peer)), CSP_ERR_NONE);
    }
    FprimeCsp::UdpConfig config = udpConfig();
    config.localPort = 65535;
    config.remotePort = 1;
    config.interfaceName = "UDP4567890";  // exactly CSP_IFLIST_NAME_MAX characters
    EXPECT_EQ(FprimeCsp::UdpTransport::validate(config), CSP_ERR_NONE);
}

void expectOpenRefusedBeforeAnything(const FprimeCsp::UdpConfig& config) {
    FakeUdpSockets sockets;
    FprimeCsp::UdpTransport transport(config, sockets);
    FprimeCsp::Stack stack;

    EXPECT_FALSE(stack.initialize(stackConfig(), transport));
    EXPECT_EQ(stack.lastError(), CSP_ERR_INVAL);
    EXPECT_EQ(sockets.openCalls, 0);
    EXPECT_EQ(Os::Test::taskStartCalls, static_cast<std::size_t>(0));
    EXPECT_FALSE(transport.isOpen());
}

void udp004OpenRefusesAnInvalidPeerBeforeAnySocket() {
    reset();
    expectOpenRefusedBeforeAnything(withPeer("192.168.70.256"));
    EXPECT_TRUE(csp_iflist_get_by_name("UDP") == nullptr);
}

void udp004OpenRefusesARegisteredNameBeforeAnySocket() {
    reset();
    csp_iface_t other{};
    other.name = "UDP";
    csp_iflist_add(&other);
    expectOpenRefusedBeforeAnything(udpConfig());
    EXPECT_TRUE(csp_iflist_get_by_name("UDP") == &other);
}

void udp004BoundaryConfigurationIsAccepted() {
    reset();
    FakeUdpSockets sockets;
    FprimeCsp::UdpConfig config;
    config.peer = "1.0.0.1";
    config.localPort = 65535;
    config.remotePort = 1;
    config.interfaceName = "UDP4567890";  // exactly CSP_IFLIST_NAME_MAX characters
    config.defaultRoute = false;
    FprimeCsp::UdpTransport transport(config, sockets);
    FprimeCsp::Stack stack;

    EXPECT_TRUE(stack.initialize(stackConfig(), transport));
    EXPECT_EQ(sockets.openPort, static_cast<std::uint16_t>(65535));
    EXPECT_EQ(std::string(transport.interface()->name), std::string("UDP4567890"));
    EXPECT_EQ(static_cast<int>(transport.interface()->is_default), 0);
    EXPECT_EQ(sendThrough(transport, {9}), CSP_ERR_NONE);
    EXPECT_EQ(sockets.sent[0].address, 0x01000001U);
    EXPECT_EQ(sockets.sent[0].port, static_cast<std::uint16_t>(1));
    stack.shutdown();
}

void udp004OpenRegistersTheConfiguredInterface() {
    reset();
    FakeUdpSockets sockets;
    FprimeCsp::UdpTransport transport(udpConfig(), sockets);
    FprimeCsp::Stack stack;

    EXPECT_TRUE(stack.initialize(stackConfig(), transport));
    EXPECT_TRUE(transport.isOpen());
    EXPECT_TRUE(csp_iflist_get_by_name("UDP") == transport.interface());
    EXPECT_EQ(transport.interface()->addr, static_cast<std::uint16_t>(6061));
    EXPECT_EQ(transport.interface()->netmask, static_cast<std::uint16_t>(8));
    EXPECT_EQ(static_cast<int>(transport.interface()->is_default), 1);
    EXPECT_EQ(Os::Test::lastTaskName, std::string("UdpReceive"));
    stack.shutdown();
}

// --- FCL-UDP-005: a failed open leaves nothing behind ---------------------------------

void udp005SocketFailureLeavesNoInterface() {
    reset();
    FakeUdpSockets sockets;
    sockets.openStatus = -EADDRINUSE;
    FprimeCsp::UdpTransport transport(udpConfig(), sockets);
    FprimeCsp::Stack stack;

    EXPECT_FALSE(stack.initialize(stackConfig(), transport));
    EXPECT_EQ(stack.lastError(), CSP_ERR_DRIVER);
    EXPECT_EQ(transport.socketError(), EADDRINUSE);
    EXPECT_FALSE(transport.isOpen());
    EXPECT_TRUE(csp_iflist_get_by_name("UDP") == nullptr);
    EXPECT_EQ(Os::Test::taskStartCalls, static_cast<std::size_t>(0));
    EXPECT_EQ(sockets.closeCalls, 0);
}

void udp005TaskFailureClosesTheSocket() {
    reset();
    Os::Test::taskStartStatus = Os::Task::ERROR_RESOURCES;
    FakeUdpSockets sockets;
    FprimeCsp::UdpTransport transport(udpConfig(), sockets);
    FprimeCsp::Stack stack;

    EXPECT_FALSE(stack.initialize(stackConfig(), transport));
    EXPECT_EQ(stack.lastError(), CSP_ERR_DRIVER);
    EXPECT_FALSE(transport.isOpen());
    EXPECT_TRUE(csp_iflist_get_by_name("UDP") == nullptr);
    EXPECT_EQ(sockets.openCalls, 1);
    EXPECT_EQ(sockets.closeCalls, 1);
}

void udp005TaskFailureWithAFailingSocketCloseIsReported() {
    reset();
    Os::Test::taskStartStatus = Os::Task::ERROR_RESOURCES;
    FakeUdpSockets sockets;
    sockets.closeStatus = -EIO;
    FprimeCsp::UdpTransport transport(udpConfig(), sockets);
    FprimeCsp::Stack stack;

    EXPECT_FALSE(stack.initialize(stackConfig(), transport));
    EXPECT_EQ(stack.lastError(), CSP_ERR_DRIVER);
    EXPECT_EQ(transport.socketError(), EIO);
    EXPECT_EQ(sockets.closeCalls, 1);
    EXPECT_TRUE(csp_iflist_get_by_name("UDP") == nullptr);
}

// --- FCL-UDP-006: ordered, bounded and retryable close --------------------------------

void udp006CloseUnregistersThenStopsReceivingThenClosesTheSocket() {
    reset();
    FakeUdpSockets sockets;
    FprimeCsp::UdpTransport transport(udpConfig(), sockets);
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(stackConfig(), transport));

    EXPECT_TRUE(stack.shutdown() == FprimeCsp::ShutdownStatus::COMPLETE);
    const auto& order = TestDoubles::cspState().closeOrder;
    EXPECT_EQ(order.size(), static_cast<std::size_t>(2));
    EXPECT_EQ(order[0], std::string("remove"));
    EXPECT_EQ(order[1], std::string("close"));
    EXPECT_EQ(Os::Test::taskJoinCalls, static_cast<std::size_t>(1));
    EXPECT_FALSE(transport.isOpen());
    EXPECT_TRUE(csp_iflist_get_by_name("UDP") == nullptr);
}

void udp006CloseIsBoundedByTheReceiveTimeout() {
    reset();
    FakeUdpSockets sockets;
    FprimeCsp::UdpTransport transport(udpConfig(), sockets);
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(stackConfig(), transport));

    EXPECT_TRUE(sockets.openTimeoutMs > 0U);
    EXPECT_TRUE(sockets.openTimeoutMs <= 1000U);
    const auto started = std::chrono::steady_clock::now();
    EXPECT_TRUE(stack.shutdown() == FprimeCsp::ShutdownStatus::COMPLETE);
    const auto took = std::chrono::steady_clock::now() - started;
    EXPECT_TRUE(took < std::chrono::milliseconds(sockets.openTimeoutMs + 500));
}

void udp006NothingReachesTheRouterAfterClose() {
    reset();
    FakeUdpSockets sockets;
    FprimeCsp::UdpTransport transport(udpConfig(), sockets);
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(stackConfig(), transport));

    EXPECT_TRUE(stack.shutdown() == FprimeCsp::ShutdownStatus::COMPLETE);
    sockets.arrive(datagram(3));
    std::this_thread::sleep_for(std::chrono::milliseconds(2 * sockets.openTimeoutMs));
    EXPECT_EQ(TestDoubles::cspState().deliveredCount.load(), 0);
}

void udp006FailedTaskJoinIsRetriedWithoutUnregisteringTwice() {
    reset();
    FakeUdpSockets sockets;
    FprimeCsp::UdpTransport transport(udpConfig(), sockets);
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(stackConfig(), transport));

    Os::Test::taskJoinStatus = Os::Task::JOIN_ERROR;
    EXPECT_TRUE(stack.shutdown() == FprimeCsp::ShutdownStatus::TRANSPORT_CLOSE_FAILED);
    EXPECT_TRUE(transport.isOpen());
    EXPECT_EQ(sockets.closeCalls, 0);

    Os::Test::taskJoinStatus = Os::Task::OP_OK;
    EXPECT_TRUE(stack.shutdown() == FprimeCsp::ShutdownStatus::COMPLETE);
    EXPECT_FALSE(transport.isOpen());
    EXPECT_EQ(sockets.closeCalls, 1);
    EXPECT_EQ(Os::Test::taskJoinCalls, static_cast<std::size_t>(2));
    const auto& order = TestDoubles::cspState().closeOrder;
    EXPECT_EQ(std::count(order.begin(), order.end(), std::string("remove")), 1);
}

void udp006FailedSocketCloseIsRetriedWithoutStoppingTwice() {
    reset();
    FakeUdpSockets sockets;
    sockets.closeStatus = -EIO;
    FprimeCsp::UdpTransport transport(udpConfig(), sockets);
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(stackConfig(), transport));

    EXPECT_TRUE(stack.shutdown() == FprimeCsp::ShutdownStatus::TRANSPORT_CLOSE_FAILED);
    EXPECT_TRUE(transport.isOpen());
    EXPECT_EQ(transport.socketError(), EIO);

    sockets.closeStatus = 0;
    EXPECT_TRUE(stack.shutdown() == FprimeCsp::ShutdownStatus::COMPLETE);
    EXPECT_FALSE(transport.isOpen());
    EXPECT_EQ(sockets.closeCalls, 2);
    EXPECT_EQ(Os::Test::taskJoinCalls, static_cast<std::size_t>(1));
}

}  // namespace

int main(int argc, char** argv) {
    return TestSupport::runSelected(
        argc, argv,
        {
            {"udp001_send_is_header_and_data_only", udp001SendIsHeaderAndDataOnly},
            {"udp001_empty_packet_is_the_header_alone", udp001EmptyPacketIsTheHeaderAlone},
            {"udp001_received_datagram_reaches_the_router", udp001ReceivedDatagramReachesTheRouter},
            {"udp002_empty_datagram_is_rejected", udp002EmptyDatagramIsRejected},
            {"udp002_datagram_one_byte_shorter_than_the_header_is_rejected",
             udp002DatagramOneByteShorterThanTheHeaderIsRejected},
            {"udp002_datagram_one_byte_longer_than_the_largest_packet_is_rejected",
             udp002DatagramOneByteLongerThanTheLargestPacketIsRejected},
            {"udp002_much_longer_datagram_is_rejected", udp002MuchLongerDatagramIsRejected},
            {"udp002_datagram_from_another_sender_is_rejected", udp002DatagramFromAnotherSenderIsRejected},
            {"udp002_header_the_stack_refuses_is_rejected", udp002HeaderTheStackRefusesIsRejected},
            {"udp002_boundary_sizes_are_delivered", udp002BoundarySizesAreDelivered},
            {"udp002_datagram_without_a_free_buffer_is_dropped", udp002DatagramWithoutAFreeBufferIsDropped},
            {"udp002_receive_error_does_not_stop_receiving", udp002ReceiveErrorDoesNotStopReceiving},
            {"udp003_send_goes_to_the_peer_address_and_port", udp003SendGoesToThePeerAddressAndPort},
            {"udp003_sent_packet_is_released_once", udp003SentPacketIsReleasedOnce},
            {"udp003_send_error_leaves_the_packet_to_libcsp", udp003SendErrorLeavesThePacketToLibcsp},
            {"udp003_partial_send_is_a_failure", udp003PartialSendIsAFailure},
            {"udp004_peer_that_is_not_an_address_is_invalid", udp004PeerThatIsNotAnAddressIsInvalid},
            {"udp004_peer_that_is_not_unicast_is_invalid", udp004PeerThatIsNotUnicastIsInvalid},
            {"udp004_zero_ports_are_invalid", udp004ZeroPortsAreInvalid},
            {"udp004_interface_names_out_of_bounds_are_invalid", udp004InterfaceNamesOutOfBoundsAreInvalid},
            {"udp004_registered_interface_name_is_invalid", udp004RegisteredInterfaceNameIsInvalid},
            {"udp004_valid_boundaries_are_accepted", udp004ValidBoundariesAreAccepted},
            {"udp004_open_refuses_an_invalid_peer_before_any_socket", udp004OpenRefusesAnInvalidPeerBeforeAnySocket},
            {"udp004_open_refuses_a_registered_name_before_any_socket",
             udp004OpenRefusesARegisteredNameBeforeAnySocket},
            {"udp004_boundary_configuration_is_accepted", udp004BoundaryConfigurationIsAccepted},
            {"udp004_open_registers_the_configured_interface", udp004OpenRegistersTheConfiguredInterface},
            {"udp005_socket_failure_leaves_no_interface", udp005SocketFailureLeavesNoInterface},
            {"udp005_task_failure_closes_the_socket", udp005TaskFailureClosesTheSocket},
            {"udp005_task_failure_with_a_failing_socket_close_is_reported",
             udp005TaskFailureWithAFailingSocketCloseIsReported},
            {"udp006_close_unregisters_then_stops_receiving_then_closes_the_socket",
             udp006CloseUnregistersThenStopsReceivingThenClosesTheSocket},
            {"udp006_close_is_bounded_by_the_receive_timeout", udp006CloseIsBoundedByTheReceiveTimeout},
            {"udp006_nothing_reaches_the_router_after_close", udp006NothingReachesTheRouterAfterClose},
            {"udp006_failed_task_join_is_retried_without_unregistering_twice",
             udp006FailedTaskJoinIsRetriedWithoutUnregisteringTwice},
            {"udp006_failed_socket_close_is_retried_without_stopping_twice",
             udp006FailedSocketCloseIsRetriedWithoutStoppingTwice},
        });
}
