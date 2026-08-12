#include "FprimeCsp/Libparam/Libparam.hpp"

#include "FprimeCsp/Libparam/libparam_bridge.h"

namespace FprimeCsp {

bool Registry::add(param_t& parameter) {
    this->m_lastError = fprimecsp_param_list_add(&parameter);
    return this->m_lastError == 0;
}

int Registry::lastError() const {
    return this->m_lastError;
}

bool Server::bind(Stack& stack) {
    if (this->m_bound) {
        return true;
    }
    if (!stack.bindCallback(fprimecsp_param_serve, FPRIMECSP_PARAM_SERVER_PORT)) {
        this->m_lastError = stack.lastError();
        return false;
    }
    this->m_bound = true;
    this->m_lastError = 0;
    return true;
}

bool Server::isBound() const {
    return this->m_bound;
}

int Server::lastError() const {
    return this->m_lastError;
}

Client::Client(const std::uint8_t protocolVersion) : m_protocolVersion(protocolVersion) {}

bool Client::pullU32(param_t& parameter,
                     const std::uint16_t peerNode,
                     const std::uint32_t timeoutMs,
                     std::uint32_t& value) {
    if ((this->m_protocolVersion == 0U) || (this->m_protocolVersion > 2U)) {
        this->m_lastError = -1;
        return false;
    }
    this->m_lastError = fprimecsp_param_pull_u32(
        &parameter, peerNode, timeoutMs, this->m_protocolVersion, &value);
    return this->m_lastError == 0;
}

bool Client::pushU32(param_t& parameter,
                     const std::uint16_t peerNode,
                     const std::uint32_t timeoutMs,
                     const std::uint32_t value) {
    if ((this->m_protocolVersion == 0U) || (this->m_protocolVersion > 2U)) {
        this->m_lastError = -1;
        return false;
    }
    this->m_lastError = fprimecsp_param_push_u32(
        &parameter, peerNode, timeoutMs, this->m_protocolVersion, value);
    return this->m_lastError == 0;
}

int Client::lastError() const {
    return this->m_lastError;
}

}  // namespace FprimeCsp
