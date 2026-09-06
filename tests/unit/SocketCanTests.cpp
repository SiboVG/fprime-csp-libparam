#include "FprimeCsp/SocketCan/SocketCanTransport.hpp"

#include "TestDoubles.hpp"
#include "TestSupport.hpp"

#include <cstdint>

namespace {

FprimeCsp::StackConfig validStackConfig() {
    FprimeCsp::StackConfig config;
    config.localNode = 42;
    config.networkPrefix = 8;
    return config;
}

void openAppliesSocketCanConfiguration() {
    TestDoubles::resetCsp();
    FprimeCsp::SocketCanConfig config;
    config.device = "can7";
    config.interfaceName = "PAYLOAD";
    config.bitrate = 500000;
    config.promiscuous = false;
    config.defaultRoute = true;
    FprimeCsp::SocketCanTransport transport(config);
    FprimeCsp::Stack stack;

    EXPECT_TRUE(stack.initialize(validStackConfig(), transport));
    const auto& state = TestDoubles::cspState();
    EXPECT_EQ(state.socketOpenCalls, 1);
    EXPECT_EQ(state.socketDevice, std::string("can7"));
    EXPECT_FALSE(state.socketInterfaceNameWasNull);
    EXPECT_EQ(state.socketInterfaceName, std::string("PAYLOAD"));
    EXPECT_EQ(state.socketNode, 42U);
    EXPECT_EQ(state.socketBitrate, 500000);
    EXPECT_FALSE(state.socketPromiscuous);
    EXPECT_TRUE(transport.isOpen());
    EXPECT_TRUE(transport.interface() == &TestDoubles::cspState().iface);
    EXPECT_EQ(transport.interface()->netmask, static_cast<std::uint16_t>(8));
    EXPECT_EQ(static_cast<int>(transport.interface()->is_default), 1);
}

void emptyInterfaceNameUsesLibcspDefault() {
    TestDoubles::resetCsp();
    FprimeCsp::SocketCanConfig config;
    config.device = "can0";
    config.interfaceName.clear();
    config.defaultRoute = false;
    FprimeCsp::SocketCanTransport transport(config);
    FprimeCsp::Stack stack;

    EXPECT_TRUE(stack.initialize(validStackConfig(), transport));
    EXPECT_TRUE(TestDoubles::cspState().socketInterfaceNameWasNull);
    EXPECT_EQ(static_cast<int>(transport.interface()->is_default), 0);
}

void driverFailureIsPreserved() {
    TestDoubles::resetCsp();
    TestDoubles::cspState().socketOpenStatus = CSP_ERR_DRIVER;
    FprimeCsp::SocketCanTransport transport({});
    FprimeCsp::Stack stack;

    EXPECT_FALSE(stack.initialize(validStackConfig(), transport));
    EXPECT_EQ(stack.lastError(), CSP_ERR_DRIVER);
    EXPECT_FALSE(transport.isOpen());
}

void successWithoutInterfaceIsRejected() {
    TestDoubles::resetCsp();
    TestDoubles::cspState().socketReturnInterface = nullptr;
    FprimeCsp::SocketCanTransport transport({});
    FprimeCsp::Stack stack;

    EXPECT_FALSE(stack.initialize(validStackConfig(), transport));
    EXPECT_EQ(stack.lastError(), CSP_ERR_INVAL);
    EXPECT_FALSE(transport.isOpen());
}

void closeRemovesThenStopsInterface() {
    TestDoubles::resetCsp();
    FprimeCsp::SocketCanTransport transport({});
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(validStackConfig(), transport));

    stack.shutdown();
    EXPECT_FALSE(transport.isOpen());
    EXPECT_EQ(TestDoubles::cspState().removeCalls, 1);
    EXPECT_EQ(TestDoubles::cspState().stopCalls, 1);
    EXPECT_EQ(TestDoubles::cspState().closeOrder.size(), static_cast<std::size_t>(2));
    EXPECT_EQ(TestDoubles::cspState().closeOrder[0], std::string("remove"));
    EXPECT_EQ(TestDoubles::cspState().closeOrder[1], std::string("stop"));
}

void stopFailureKeepsInterfaceForRetry() {
    TestDoubles::resetCsp();
    TestDoubles::cspState().stopStatus = CSP_ERR_DRIVER;
    FprimeCsp::SocketCanTransport transport({});
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(validStackConfig(), transport));

    EXPECT_TRUE(stack.shutdown() == FprimeCsp::ShutdownStatus::TRANSPORT_CLOSE_FAILED);
    EXPECT_TRUE(transport.isOpen());
    EXPECT_EQ(stack.lastError(), CSP_ERR_DRIVER);

    TestDoubles::cspState().stopStatus = CSP_ERR_NONE;
    EXPECT_TRUE(stack.shutdown() == FprimeCsp::ShutdownStatus::COMPLETE);
    EXPECT_FALSE(transport.isOpen());
    EXPECT_EQ(TestDoubles::cspState().removeCalls, 1);
    EXPECT_EQ(TestDoubles::cspState().stopCalls, 2);
}

void removeFailureDefersDriverStopUntilRetry() {
    TestDoubles::resetCsp();
    TestDoubles::cspState().removeStatus = CSP_ERR_DRIVER;
    FprimeCsp::SocketCanTransport transport({});
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(validStackConfig(), transport));

    EXPECT_TRUE(stack.shutdown() == FprimeCsp::ShutdownStatus::TRANSPORT_CLOSE_FAILED);
    EXPECT_TRUE(transport.isOpen());
    EXPECT_EQ(TestDoubles::cspState().removeCalls, 1);
    EXPECT_EQ(TestDoubles::cspState().stopCalls, 0);

    TestDoubles::cspState().removeStatus = CSP_ERR_NONE;
    EXPECT_TRUE(stack.shutdown() == FprimeCsp::ShutdownStatus::COMPLETE);
    EXPECT_FALSE(transport.isOpen());
    EXPECT_EQ(TestDoubles::cspState().removeCalls, 2);
    EXPECT_EQ(TestDoubles::cspState().stopCalls, 1);
}

void emptyDeviceIsRejectedBeforeDriverCall() {
    TestDoubles::resetCsp();
    FprimeCsp::SocketCanConfig config;
    config.device.clear();
    FprimeCsp::SocketCanTransport transport(config);
    FprimeCsp::Stack stack;

    EXPECT_FALSE(stack.initialize(validStackConfig(), transport));
    EXPECT_EQ(stack.lastError(), CSP_ERR_INVAL);
    EXPECT_EQ(TestDoubles::cspState().socketOpenCalls, 0);
    EXPECT_FALSE(transport.isOpen());
}

}  // namespace

int main(int argc, char** argv) {
    return TestSupport::runSelected(
        argc,
        argv,
        {{"open_applies_socketcan_configuration", openAppliesSocketCanConfiguration},
         {"empty_interface_name_uses_libcsp_default", emptyInterfaceNameUsesLibcspDefault},
         {"driver_failure_is_preserved", driverFailureIsPreserved},
         {"success_without_interface_is_rejected", successWithoutInterfaceIsRejected},
         {"close_removes_then_stops_interface", closeRemovesThenStopsInterface},
         {"stop_failure_keeps_interface_for_retry", stopFailureKeepsInterfaceForRetry},
         {"remove_failure_defers_driver_stop_until_retry", removeFailureDefersDriverStopUntilRetry},
         {"empty_device_is_rejected_before_driver_call", emptyDeviceIsRejectedBeforeDriverCall}});
}
