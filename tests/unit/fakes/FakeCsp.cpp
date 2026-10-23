#include "TestDoubles.hpp"

#include <algorithm>
#include <cstring>
#include <thread>

extern "C" {
#include <csp/csp_buffer.h>
#include <csp/csp_id.h>
#include <csp/csp_iflist.h>
}

namespace TestDoubles {
namespace {
CspState state;
}

CspState& cspState() {
    return state;
}

void resetCsp() {
    state.initCalls = 0;
    state.bindCalls = 0;
    state.boundCallback = nullptr;
    state.boundPort = 0;
    state.bindStatus = CSP_ERR_NONE;
    state.routeCalls.store(0);
    state.pingCalls = 0;
    state.pingNode = 0;
    state.pingTimeout = 0;
    state.pingSize = 0;
    state.pingOptions = 0;
    state.pingStatus = 0;
    state.serviceCalls = 0;
    state.servicePacket = nullptr;
    state.socketOpenCalls = 0;
    state.socketDevice.clear();
    state.socketInterfaceName.clear();
    state.socketInterfaceNameWasNull = false;
    state.socketNode = 0;
    state.socketBitrate = 0;
    state.socketPromiscuous = false;
    state.socketOpenStatus = CSP_ERR_NONE;
    state.iface = {};
    state.socketReturnInterface = &state.iface;
    state.removeCalls = 0;
    state.removeStatus = CSP_ERR_NONE;
    state.stopCalls = 0;
    state.stopStatus = CSP_ERR_NONE;
    state.closeOrder.clear();
    state.buffersInUse.store(0);
    state.bufferFreeCalls.store(0);
    state.bufferGetFailures.store(0);
    state.stripFailures.store(0);
    {
        const std::lock_guard<std::mutex> lock(state.deliveredMutex);
        state.delivered.clear();
    }
    state.deliveredCount.store(0);
    state.interfaces.clear();
    csp_conf = {2, "", "", "", CSP_O_NONE, CSP_DEDUP_OFF};
}
}  // namespace TestDoubles

extern "C" {

csp_conf_t csp_conf = {2, "", "", "", CSP_O_NONE, CSP_DEDUP_OFF};

void csp_init(void) {
    ++TestDoubles::cspState().initCalls;
}

int csp_bind_callback(csp_callback_t callback, std::uint8_t port) {
    auto& state = TestDoubles::cspState();
    ++state.bindCalls;
    state.boundCallback = callback;
    state.boundPort = port;
    return state.bindStatus;
}

int csp_route_work(void) {
    ++TestDoubles::cspState().routeCalls;
    std::this_thread::yield();
    return CSP_ERR_NONE;
}

int csp_ping(std::uint16_t node, std::uint32_t timeout, unsigned int size, std::uint8_t options) {
    auto& state = TestDoubles::cspState();
    ++state.pingCalls;
    state.pingNode = node;
    state.pingTimeout = timeout;
    state.pingSize = size;
    state.pingOptions = options;
    return state.pingStatus;
}

void csp_service_handler(csp_packet_t* packet) {
    auto& state = TestDoubles::cspState();
    ++state.serviceCalls;
    state.servicePacket = packet;
}

int csp_can_socketcan_open_and_add_interface(const char* device,
                                             const char* interfaceName,
                                             unsigned int node,
                                             int bitrate,
                                             bool promiscuous,
                                             csp_iface_t** returnInterface) {
    auto& state = TestDoubles::cspState();
    ++state.socketOpenCalls;
    state.socketDevice = device == nullptr ? "" : device;
    state.socketInterfaceNameWasNull = interfaceName == nullptr;
    state.socketInterfaceName = interfaceName == nullptr ? "" : interfaceName;
    state.socketNode = node;
    state.socketBitrate = bitrate;
    state.socketPromiscuous = promiscuous;
    if ((state.socketOpenStatus == CSP_ERR_NONE) && (returnInterface != nullptr)) {
        *returnInterface = state.socketReturnInterface;
    }
    return state.socketOpenStatus;
}

int csp_can_remove_interface(csp_iface_t*) {
    auto& state = TestDoubles::cspState();
    ++state.removeCalls;
    state.closeOrder.emplace_back("remove");
    return state.removeStatus;
}

int csp_can_socketcan_stop(csp_iface_t*) {
    auto& state = TestDoubles::cspState();
    ++state.stopCalls;
    state.closeOrder.emplace_back("stop");
    return state.stopStatus;
}

csp_packet_t* csp_buffer_get(size_t) {
    auto& state = TestDoubles::cspState();
    if (state.bufferGetFailures.load() > 0) {
        --state.bufferGetFailures;
        return nullptr;
    }
    ++state.buffersInUse;
    return new csp_packet_t{};
}

void csp_buffer_free(void* buffer) {
    auto& state = TestDoubles::cspState();
    ++state.bufferFreeCalls;
    --state.buffersInUse;
    delete static_cast<csp_packet_t*>(buffer);
}

void csp_id_prepend(csp_packet_t* packet) {
    packet->frame_begin = packet->data - sizeof(TestDoubles::FAKE_HEADER);
    std::memcpy(packet->frame_begin, TestDoubles::FAKE_HEADER, sizeof(TestDoubles::FAKE_HEADER));
    packet->frame_length = static_cast<std::uint16_t>(packet->length + sizeof(TestDoubles::FAKE_HEADER));
}

int csp_id_setup_rx(csp_packet_t* packet) {
    packet->frame_begin = packet->data - sizeof(TestDoubles::FAKE_HEADER);
    packet->frame_length = 0;
    return static_cast<int>(sizeof(TestDoubles::FAKE_HEADER));
}

int csp_id_strip(csp_packet_t* packet) {
    auto& state = TestDoubles::cspState();
    if (state.stripFailures.load() > 0) {
        --state.stripFailures;
        return -1;
    }
    if (packet->frame_length < sizeof(TestDoubles::FAKE_HEADER)) {
        return -1;
    }
    packet->length = static_cast<std::uint16_t>(packet->frame_length - sizeof(TestDoubles::FAKE_HEADER));
    return 0;
}

void csp_qfifo_write(csp_packet_t* packet, csp_iface_t* iface, void*) {
    auto& state = TestDoubles::cspState();
    {
        const std::lock_guard<std::mutex> lock(state.deliveredMutex);
        TestDoubles::CspState::Delivered entry;
        entry.frame.assign(packet->frame_begin, packet->frame_begin + packet->frame_length);
        entry.data.assign(packet->data, packet->data + packet->length);
        entry.iface = iface;
        state.delivered.push_back(std::move(entry));
    }
    ++state.deliveredCount;
    csp_buffer_free(packet);
}

void csp_iflist_add(csp_iface_t* iface) {
    auto& interfaces = TestDoubles::cspState().interfaces;
    for (const csp_iface_t* existing : interfaces) {
        if ((existing == iface) || (std::strncmp(existing->name, iface->name, CSP_IFLIST_NAME_MAX) == 0)) {
            return;  // as libcsp: a duplicate is ignored silently
        }
    }
    interfaces.push_back(iface);
}

void csp_iflist_remove(csp_iface_t* iface) {
    auto& state = TestDoubles::cspState();
    state.closeOrder.emplace_back("remove");
    state.interfaces.erase(std::remove(state.interfaces.begin(), state.interfaces.end(), iface),
                           state.interfaces.end());
}

csp_iface_t* csp_iflist_get_by_name(const char* name) {
    for (csp_iface_t* iface : TestDoubles::cspState().interfaces) {
        if (std::strncmp(iface->name, name, CSP_IFLIST_NAME_MAX) == 0) {
            return iface;
        }
    }
    return nullptr;
}

}  // extern "C"
