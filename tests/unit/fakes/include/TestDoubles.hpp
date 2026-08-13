#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

extern "C" {
#include <csp/csp.h>
#include <csp/csp_interface.h>
#include <param/param.h>
}

namespace TestDoubles {

struct CspState {
    int initCalls = 0;
    int bindCalls = 0;
    csp_callback_t boundCallback = nullptr;
    std::uint8_t boundPort = 0;
    int bindStatus = CSP_ERR_NONE;
    std::atomic<int> routeCalls{0};
    int pingCalls = 0;
    std::uint16_t pingNode = 0;
    std::uint32_t pingTimeout = 0;
    unsigned int pingSize = 0;
    std::uint8_t pingOptions = 0;
    int pingStatus = 0;
    int serviceCalls = 0;
    csp_packet_t* servicePacket = nullptr;

    int socketOpenCalls = 0;
    std::string socketDevice;
    std::string socketInterfaceName;
    bool socketInterfaceNameWasNull = false;
    unsigned int socketNode = 0;
    int socketBitrate = 0;
    bool socketPromiscuous = false;
    int socketOpenStatus = CSP_ERR_NONE;
    csp_iface_t iface{};
    csp_iface_t* socketReturnInterface = &iface;
    int removeCalls = 0;
    int removeStatus = CSP_ERR_NONE;
    int stopCalls = 0;
    int stopStatus = CSP_ERR_NONE;
    std::vector<std::string> closeOrder;
};

struct ParamState {
    int listAddCalls = 0;
    param_t* listParameter = nullptr;
    int listAddStatus = 0;
    int serveCalls = 0;
    csp_packet_t* servedPacket = nullptr;
    int pullCalls = 0;
    param_t* pullParameter = nullptr;
    int pullOffset = 0;
    int pullPriority = 0;
    int pullVerbose = 0;
    int pullHost = 0;
    int pullTimeout = 0;
    int pullVersion = 0;
    int pullStatus = 0;
    int getU32Calls = 0;
    std::uint32_t getU32Value = 0;
    int getIntegerCalls = 0;
    param_type_e getIntegerType = PARAM_TYPE_INVALID;
    unsigned int getIntegerIndex = 0;
    std::int64_t getIntegerValue = 0;
    int pushCalls = 0;
    param_t* pushParameter = nullptr;
    int pushOffset = 0;
    int pushPriority = 0;
    int pushVerbose = 0;
    int pushHost = 0;
    int pushTimeout = 0;
    int pushVersion = 0;
    bool pushAckWithPull = false;
    std::uint32_t pushedValue = 0;
    std::int64_t pushedIntegerValue = 0;
    int pushStatus = 0;
};

CspState& cspState();
ParamState& paramState();
void resetCsp();
void resetParam();

}  // namespace TestDoubles
