#include "FprimeCsp/SocketCan/SocketCanTransport.hpp"

#include <utility>

extern "C" {
#include <csp/drivers/can_socketcan.h>
#include <csp/interfaces/csp_if_can.h>
}

namespace FprimeCsp {

SocketCanTransport::SocketCanTransport(SocketCanConfig config) : m_config(std::move(config)) {}

SocketCanTransport::~SocketCanTransport() {
    (void)this->close();
}

int SocketCanTransport::open(const std::uint16_t localNode, const std::uint8_t networkPrefix) {
    if (this->m_interface != nullptr) {
        return CSP_ERR_ALREADY;
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

    const int removeStatus = csp_can_remove_interface(this->m_interface);
    const int stopStatus = csp_can_socketcan_stop(this->m_interface);
    if (stopStatus == CSP_ERR_NONE) {
        this->m_interface = nullptr;
    }
    return (removeStatus != CSP_ERR_NONE) ? removeStatus : stopStatus;
}

bool SocketCanTransport::isOpen() const {
    return this->m_interface != nullptr;
}

csp_iface_t* SocketCanTransport::interface() const {
    return this->m_interface;
}

}  // namespace FprimeCsp
