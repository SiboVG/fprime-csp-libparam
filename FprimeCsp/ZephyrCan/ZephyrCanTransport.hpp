#ifndef FPRIMECSP_ZEPHYRCAN_ZEPHYRCANTRANSPORT_HPP
#define FPRIMECSP_ZEPHYRCAN_ZEPHYRCANTRANSPORT_HPP

#include "FprimeCsp/Runtime/Stack.hpp"

#include <cstdint>
#include <string>

struct device;

namespace FprimeCsp {

/** Zephyr CAN settings passed to libcsp csh/2.10's Zephyr CAN driver. */
struct ZephyrCanConfig {
    /** Zephyr CAN controller, e.g. DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus)); required. */
    const struct device* device = nullptr;
    std::string interfaceName = "CAN";
    /** Bitrate applied with can_set_bitrate(); required. */
    std::uint32_t bitrate = 0;
    /** Receive every CSP frame; otherwise only frames addressed to the local node. */
    bool promiscuous = true;
    bool defaultRoute = true;
};

/** Zephyr CAN transport with the same interface ownership as SocketCanTransport. */
class ZephyrCanTransport final : public Transport {
  public:
    explicit ZephyrCanTransport(ZephyrCanConfig config);
    ~ZephyrCanTransport() override;

    ZephyrCanTransport(const ZephyrCanTransport&) = delete;
    ZephyrCanTransport& operator=(const ZephyrCanTransport&) = delete;

    bool isOpen() const;
    csp_iface_t* interface() const;

  private:
    int open(std::uint16_t localNode, std::uint8_t networkPrefix) override;
    int close() override;

    ZephyrCanConfig m_config;
    csp_iface_t* m_interface = nullptr;
};

}  // namespace FprimeCsp

#endif  // FPRIMECSP_ZEPHYRCAN_ZEPHYRCANTRANSPORT_HPP
