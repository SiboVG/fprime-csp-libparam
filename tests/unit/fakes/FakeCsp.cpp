#include "TestDoubles.hpp"

#include <cstring>
#include <thread>

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

}  // extern "C"
