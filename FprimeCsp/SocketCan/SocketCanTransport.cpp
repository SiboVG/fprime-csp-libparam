#include "FprimeCsp/SocketCan/SocketCanTransport.hpp"

#include <exception>
#include <utility>

extern "C" {
#include <csp/drivers/can_socketcan.h>
#include <csp/interfaces/csp_if_can.h>
}

namespace FprimeCsp {

SocketCanTransport::SocketCanTransport(SocketCanConfig config) : m_config(std::move(config)) {}

SocketCanTransport::~SocketCanTransport() {
    if (this->close() != CSP_ERR_NONE) {
        // Returning could destroy configuration and driver ownership while a
        // registered CSP interface still references them.
        std::terminate();
    }
}

int SocketCanTransport::open(const std::uint16_t localNode, const std::uint8_t networkPrefix) {
    if (this->m_interface != nullptr) {
        return CSP_ERR_ALREADY;
    }
    if (this->m_config.device.empty()) {
        return CSP_ERR_INVAL;
    }

    const char* interfaceName = this->m_config.interfaceName.empty() ? nullptr : this->m_config.interfaceName.c_str();
    const int status = csp_can_socketcan_open_and_add_interface(this->m_config.device.c_str(),
                                                                interfaceName,
                                                                localNode,
                                                                this->m_config.bitrate,
                                                                this->m_config.promiscuous,
                                                                &this->m_interface);
    if (status != CSP_ERR_NONE) {
        this->m_interface = nullptr;
        return status;
    }
    if (this->m_interface == nullptr) {
        return CSP_ERR_INVAL;
    }

    this->m_interfaceRegistered = true;
    this->m_interface->netmask = networkPrefix;
    if (this->m_config.defaultRoute) {
        this->m_interface->is_default = 1;
    }
    return CSP_ERR_NONE;
}

int SocketCanTransport::close() {
    if (this->m_interface == nullptr) {
        return CSP_ERR_NONE;
    }

    if (this->m_interfaceRegistered) {
        const int removeStatus = csp_can_remove_interface(this->m_interface);
        if (removeStatus != CSP_ERR_NONE) {
            return removeStatus;
        }
        this->m_interfaceRegistered = false;
    }

    const int stopStatus = csp_can_socketcan_stop(this->m_interface);
    if (stopStatus == CSP_ERR_NONE) {
        this->m_interface = nullptr;
    }
    return stopStatus;
}

bool SocketCanTransport::isOpen() const {
    return this->m_interface != nullptr;
}

csp_iface_t* SocketCanTransport::interface() const {
    return this->m_interface;
}

}  // namespace FprimeCsp
