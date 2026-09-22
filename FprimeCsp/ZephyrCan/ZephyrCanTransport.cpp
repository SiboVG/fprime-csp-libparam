#include "FprimeCsp/ZephyrCan/ZephyrCanTransport.hpp"

#include <cerrno>
#include <exception>
#include <utility>

extern "C" {
#include <csp/drivers/can_zephyr.h>
}

namespace FprimeCsp {

namespace {

// CSP v2 CAN filter on the destination node field
constexpr std::uint16_t NODE_FILTER_MASK = 0x3FFF;

}  // namespace

ZephyrCanTransport::ZephyrCanTransport(ZephyrCanConfig config) : m_config(std::move(config)) {}

ZephyrCanTransport::~ZephyrCanTransport() {
    if (this->close() != CSP_ERR_NONE) {
        // Returning could destroy configuration and driver ownership while a
        // registered CSP interface still references them.
        std::terminate();
    }
}

bool ZephyrCanTransport::isOpen() const {
    return this->m_interface != nullptr;
}

csp_iface_t* ZephyrCanTransport::interface() const {
    return this->m_interface;
}

int ZephyrCanTransport::open(const std::uint16_t localNode, const std::uint8_t networkPrefix) {
    if (this->m_interface != nullptr) {
        return CSP_ERR_ALREADY;
    }
    if ((this->m_config.device == nullptr) || (this->m_config.bitrate == 0U)) {
        return CSP_ERR_INVAL;
    }

    const char* interfaceName = this->m_config.interfaceName.empty() ? nullptr : this->m_config.interfaceName.c_str();
    const std::uint16_t filterMask = this->m_config.promiscuous ? 0U : NODE_FILTER_MASK;
    const int status = csp_can_open_and_add_interface(this->m_config.device,
                                                      interfaceName,
                                                      localNode,
                                                      this->m_config.bitrate,
                                                      localNode,
                                                      filterMask,
                                                      &this->m_interface);
    if (status != CSP_ERR_NONE) {
        this->m_interface = nullptr;
        return status;
    }
    if (this->m_interface == nullptr) {
        return CSP_ERR_INVAL;
    }

    this->m_interface->netmask = networkPrefix;
    if (this->m_config.defaultRoute) {
        this->m_interface->is_default = 1;
    }
    return CSP_ERR_NONE;
}

int ZephyrCanTransport::close() {
    if (this->m_interface == nullptr) {
        return CSP_ERR_NONE;
    }

    // csp_can_stop() stops the controller, removes the interface from CSP and
    // frees it. Only CSP_ERR_INVAL leaves the interface in place; any other
    // result comes from can_stop() after the interface has been released.
    const int status = csp_can_stop(this->m_interface);
    if (status == CSP_ERR_INVAL) {
        return status;
    }
    this->m_interface = nullptr;
    return ((status >= 0) || (status == -EALREADY)) ? CSP_ERR_NONE : CSP_ERR_DRIVER;
}

}  // namespace FprimeCsp
