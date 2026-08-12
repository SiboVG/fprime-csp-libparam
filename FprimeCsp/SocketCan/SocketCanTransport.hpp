#pragma once

#include "FprimeCsp/Runtime/Stack.hpp"

#include <cstdint>
#include <string>

namespace FprimeCsp {

/** Linux SocketCAN settings passed to libcsp csh/2.10's six-argument API. */
struct SocketCanConfig {
    std::string device = "can0";
    std::string interfaceName = "CAN";
    int bitrate = 0;
    bool promiscuous = true;
    bool defaultRoute = true;
};

/** Linux SocketCAN transport with process-final interface ownership. */
class SocketCanTransport final : public Transport {
  public:
    explicit SocketCanTransport(SocketCanConfig config);
    ~SocketCanTransport() override;

    SocketCanTransport(const SocketCanTransport&) = delete;
    SocketCanTransport& operator=(const SocketCanTransport&) = delete;

    bool isOpen() const;
    csp_iface_t* interface() const;

  private:
    int open(std::uint16_t localNode, std::uint8_t networkPrefix) override;
    int close() override;

    SocketCanConfig m_config;
    csp_iface_t* m_interface = nullptr;
};

}  // namespace FprimeCsp
