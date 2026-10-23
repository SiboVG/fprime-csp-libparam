#include "FprimeCsp/Udp/UdpTransport.hpp"

#include <array>
#include <cerrno>
#include <cstring>
#include <exception>
#include <utility>

extern "C" {
#include <csp/csp_buffer.h>
#include <csp/csp_id.h>
#include <csp/csp_iflist.h>
}

namespace FprimeCsp {

namespace {

/** Parse a dotted-decimal IPv4 address: four fields 0..255, no leading zeros, nothing else. */
bool parseIpv4(const std::string& text, std::uint32_t& address) {
    std::uint32_t result = 0;
    std::uint32_t field = 0;
    unsigned int digits = 0;
    unsigned int fields = 0;
    for (const char c : text) {
        if ((c >= '0') && (c <= '9')) {
            if ((digits == 1U) && (field == 0U)) {
                return false;  // a leading zero
            }
            field = (field * 10U) + static_cast<std::uint32_t>(c - '0');
            ++digits;
            if (field > 255U) {
                return false;
            }
        } else if ((c == '.') && (digits > 0U) && (fields < 3U)) {
            result = (result << 8U) | field;
            field = 0;
            digits = 0;
            ++fields;
        } else {
            return false;
        }
    }
    if ((digits == 0U) || (fields != 3U)) {
        return false;
    }
    address = (result << 8U) | field;
    return true;
}

/** A unicast address: not 0.0.0.0/8, not multicast (224/4), not reserved or broadcast (240/4). */
bool isUnicast(const std::uint32_t address) {
    const std::uint32_t first = address >> 24U;
    return (first != 0U) && (first < 224U);
}

}  // namespace

UdpTransport::UdpTransport(UdpConfig config, UdpSocketApi& sockets)
    : m_config(std::move(config)), m_sockets(sockets) {}

UdpTransport::~UdpTransport() {
    if (this->close() != CSP_ERR_NONE) {
        // Returning could destroy the interface and the receive task's context while
        // libcsp or the task still references them.
        std::terminate();
    }
}

int UdpTransport::validate(const UdpConfig& config) {
    std::uint32_t peer = 0;
    if (!parseIpv4(config.peer, peer) || !isUnicast(peer)) {
        return CSP_ERR_INVAL;
    }
    if ((config.localPort == 0U) || (config.remotePort == 0U)) {
        return CSP_ERR_INVAL;
    }
    if (config.interfaceName.empty() || (config.interfaceName.size() > CSP_IFLIST_NAME_MAX)) {
        return CSP_ERR_INVAL;
    }
    // libcsp's interface list ignores a second interface of the same name without telling
    if (csp_iflist_get_by_name(config.interfaceName.c_str()) != nullptr) {
        return CSP_ERR_INVAL;
    }
    return CSP_ERR_NONE;
}

bool UdpTransport::isOpen() const {
    return this->m_registered || this->m_receiveStarted || (this->m_socket >= 0);
}

csp_iface_t* UdpTransport::interface() {
    return &this->m_interface;
}

int UdpTransport::socketError() const {
    return this->m_socketError.load();
}

int UdpTransport::open(const std::uint16_t localNode, const std::uint8_t networkPrefix) {
    // Only Stack::initialize() opens a transport, once per process (FCL-ARCH-001)
    const int status = validate(this->m_config);
    if (status != CSP_ERR_NONE) {
        return status;
    }
    (void)parseIpv4(this->m_config.peer, this->m_peer);

    const int socket = this->m_sockets.open(this->m_config.localPort, RECEIVE_TIMEOUT_MS);
    if (socket < 0) {
        this->m_socketError = -socket;
        return CSP_ERR_DRIVER;
    }
    this->m_socket = socket;

    // The interface is complete before the receive task can hand it a packet
    this->m_interface = {};
    this->m_interface.name = this->m_config.interfaceName.c_str();
    this->m_interface.addr = localNode;
    this->m_interface.netmask = networkPrefix;
    this->m_interface.driver_data = this;
    this->m_interface.nexthop = &UdpTransport::nexthop;
    this->m_interface.is_default = this->m_config.defaultRoute ? 1U : 0U;

    this->m_receiving = true;
    const Os::TaskString taskName("UdpReceive");
    const Os::Task::Arguments arguments(taskName,
                                        &UdpTransport::receiveEntry,
                                        this,
                                        this->m_config.receivePriority,
                                        this->m_config.receiveStackSize,
                                        Os::Task::TASK_DEFAULT);
    if (this->m_receiveTask.start(arguments) != Os::Task::OP_OK) {
        this->m_receiving = false;
        const int closeStatus = this->m_sockets.close(this->m_socket);
        if (closeStatus < 0) {
            this->m_socketError = -closeStatus;
        }
        this->m_socket = -1;
        return CSP_ERR_DRIVER;
    }
    this->m_receiveStarted = true;

    csp_iflist_add(&this->m_interface);
    this->m_registered = true;
    return CSP_ERR_NONE;
}

int UdpTransport::close() {
    // No datagram reaches the router once closing has begun
    this->m_receiving = false;
    if (this->m_registered) {
        csp_iflist_remove(&this->m_interface);
        this->m_registered = false;
    }
    if (this->m_receiveStarted) {
        // The task notices within one receive timeout
        if (this->m_receiveTask.join() != Os::Task::OP_OK) {
            return CSP_ERR_DRIVER;
        }
        this->m_receiveStarted = false;
    }
    if (this->m_socket >= 0) {
        const int status = this->m_sockets.close(this->m_socket);
        if (status < 0) {
            this->m_socketError = -status;
            return CSP_ERR_DRIVER;
        }
        this->m_socket = -1;
    }
    return CSP_ERR_NONE;
}

int UdpTransport::nexthop(csp_iface_t* iface, std::uint16_t via, csp_packet_t* packet, int fromMe) {
    (void)via;  // point to point: every packet goes to the peer
    (void)fromMe;
    auto* self = static_cast<UdpTransport*>(iface->driver_data);

    csp_id_prepend(packet);
    const int sent = self->m_sockets.send(self->m_socket, packet->frame_begin, packet->frame_length, self->m_peer,
                                          self->m_config.remotePort);
    if (sent != static_cast<int>(packet->frame_length)) {
        if (sent < 0) {
            self->m_socketError = -sent;
        }
        // libcsp frees the packet and counts the error
        return CSP_ERR_TX;
    }
    csp_buffer_free(packet);
    return CSP_ERR_NONE;
}

void UdpTransport::receiveEntry(void* context) {
    static_cast<UdpTransport*>(context)->receiveLoop();
}

void UdpTransport::receiveLoop() {
    // One byte more than the largest valid datagram, so a longer one is recognised
    std::array<std::uint8_t, sizeof(csp_packet_t::header) + sizeof(csp_packet_t::data) + 1U> datagram{};
    while (this->m_receiving.load()) {
        std::uint32_t sender = 0;
        const int received = this->m_sockets.receive(this->m_socket, datagram.data(), datagram.size(), sender);
        if (!this->m_receiving.load()) {
            break;
        }
        if (received == -EAGAIN) {
            continue;
        }
        if (received < 0) {
            this->m_socketError = -received;
            continue;
        }
        if (sender != this->m_peer) {
            ++this->m_interface.rx_error;
            continue;
        }
        this->deliver(datagram.data(), static_cast<std::size_t>(received));
    }
}

void UdpTransport::deliver(const std::uint8_t* datagram, const std::size_t size) {
    csp_packet_t* packet = csp_buffer_get(0);
    if (packet == nullptr) {
        ++this->m_interface.drop;
        return;
    }
    const std::size_t header = static_cast<std::size_t>(csp_id_setup_rx(packet));
    if ((size < header) || (size > (header + sizeof(packet->data)))) {
        ++this->m_interface.rx_error;
        csp_buffer_free(packet);
        return;
    }
    std::memcpy(packet->frame_begin, datagram, size);
    packet->frame_length = static_cast<std::uint16_t>(size);
    if (csp_id_strip(packet) != 0) {
        ++this->m_interface.rx_error;
        csp_buffer_free(packet);
        return;
    }
    csp_qfifo_write(packet, &this->m_interface, nullptr);
}

}  // namespace FprimeCsp
