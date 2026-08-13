#include "FprimeCsp/Libparam/Libparam.hpp"

#include "FprimeCsp/Libparam/libparam_bridge.h"
#include "TestDoubles.hpp"
#include "TestSupport.hpp"

#include <cstdint>

namespace {

class RecordingTransport final : public FprimeCsp::Transport {
  private:
    int open(std::uint16_t, std::uint8_t) override { return CSP_ERR_NONE; }
    int close() override { return CSP_ERR_NONE; }
};

FprimeCsp::StackConfig validConfig() {
    FprimeCsp::StackConfig config;
    config.localNode = 42;
    config.networkPrefix = 8;
    return config;
}

param_t descriptor() {
    static std::uint16_t node = 71;
    static std::uint32_t storage = 0;
    static csp_timestamp_t timestamp{};
    param_t parameter{};
    parameter.id = 123;
    parameter.node = &node;
    parameter.type = PARAM_TYPE_UINT32;
    parameter.addr = &storage;
    parameter.array_size = 1;
    parameter.array_step = sizeof(storage);
    parameter.timestamp = &timestamp;
    return parameter;
}

void registryForwardsDescriptorAndStatus() {
    TestDoubles::resetParam();
    auto parameter = descriptor();
    FprimeCsp::Registry registry;

    EXPECT_TRUE(registry.add(parameter));
    EXPECT_EQ(registry.lastError(), 0);
    EXPECT_EQ(TestDoubles::paramState().listAddCalls, 1);
    EXPECT_TRUE(TestDoubles::paramState().listParameter == &parameter);

    TestDoubles::paramState().listAddStatus = 1;
    EXPECT_FALSE(registry.add(parameter));
    EXPECT_EQ(registry.lastError(), 1);
}

void serverBindsStandardCallbackOnce() {
    TestDoubles::resetCsp();
    TestDoubles::resetParam();
    RecordingTransport transport;
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(validConfig(), transport));
    FprimeCsp::Server server;

    EXPECT_TRUE(server.bind(stack));
    EXPECT_TRUE(server.bind(stack));
    EXPECT_TRUE(server.isBound());
    EXPECT_EQ(server.lastError(), 0);
    EXPECT_EQ(TestDoubles::cspState().bindCalls, 1);
    EXPECT_EQ(static_cast<int>(TestDoubles::cspState().boundPort), FPRIMECSP_PARAM_SERVER_PORT);
    EXPECT_TRUE(TestDoubles::cspState().boundCallback == fprimecsp_param_serve);

    csp_packet_t packet{};
    TestDoubles::cspState().boundCallback(&packet);
    EXPECT_EQ(TestDoubles::paramState().serveCalls, 1);
    EXPECT_TRUE(TestDoubles::paramState().servedPacket == &packet);
}

void pullU32ForwardsRequestAndReadsOnSuccess() {
    TestDoubles::resetParam();
    TestDoubles::paramState().getU32Value = 0x1234ABCDU;
    auto parameter = descriptor();
    FprimeCsp::Client client(2);
    std::uint32_t value = 0;

    EXPECT_TRUE(client.pullU32(parameter, 71, 900, value));
    const auto& state = TestDoubles::paramState();
    EXPECT_EQ(client.lastError(), 0);
    EXPECT_EQ(value, 0x1234ABCDU);
    EXPECT_EQ(state.pullCalls, 1);
    EXPECT_TRUE(state.pullParameter == &parameter);
    EXPECT_EQ(state.pullOffset, -1);
    EXPECT_EQ(state.pullPriority, CSP_PRIO_NORM);
    EXPECT_EQ(state.pullVerbose, 0);
    EXPECT_EQ(state.pullHost, 71);
    EXPECT_EQ(state.pullTimeout, 900);
    EXPECT_EQ(state.pullVersion, 2);
    EXPECT_EQ(state.getU32Calls, 1);
}

void pullU32PreservesOutputOnFailure() {
    TestDoubles::resetParam();
    TestDoubles::paramState().pullStatus = -23;
    TestDoubles::paramState().getU32Value = 999;
    auto parameter = descriptor();
    FprimeCsp::Client client;
    std::uint32_t value = 77;

    EXPECT_FALSE(client.pullU32(parameter, 71, 900, value));
    EXPECT_EQ(client.lastError(), -23);
    EXPECT_EQ(value, 77U);
    EXPECT_EQ(TestDoubles::paramState().getU32Calls, 0);
}

void pushU32ForwardsValueAndAckPolicy() {
    TestDoubles::resetParam();
    auto parameter = descriptor();
    FprimeCsp::Client client(1);

    EXPECT_TRUE(client.pushU32(parameter, 55, 321, 0xDEADBEEFU));
    const auto& state = TestDoubles::paramState();
    EXPECT_EQ(state.pushCalls, 1);
    EXPECT_TRUE(state.pushParameter == &parameter);
    EXPECT_EQ(state.pushOffset, -1);
    EXPECT_EQ(state.pushPriority, CSP_PRIO_NORM);
    EXPECT_EQ(state.pushVerbose, 0);
    EXPECT_EQ(state.pushHost, 55);
    EXPECT_EQ(state.pushTimeout, 321);
    EXPECT_EQ(state.pushVersion, 1);
    EXPECT_TRUE(state.pushAckWithPull);
    EXPECT_EQ(state.pushedValue, 0xDEADBEEFU);
}

void invalidProtocolVersionIsRejected() {
    TestDoubles::resetParam();
    auto parameter = descriptor();
    FprimeCsp::Client client(0);
    std::uint32_t value = 44;

    EXPECT_FALSE(client.pullU32(parameter, 71, 100, value));
    EXPECT_FALSE(client.pushU32(parameter, 71, 100, 11));
    EXPECT_EQ(client.lastError(), -1);
    EXPECT_EQ(value, 44U);
    EXPECT_EQ(TestDoubles::paramState().pullCalls, 0);
    EXPECT_EQ(TestDoubles::paramState().pushCalls, 0);
}

void bridgeGuardsNullInputs() {
    TestDoubles::resetParam();
    std::uint32_t value = 0;
    auto parameter = descriptor();

    EXPECT_EQ(fprimecsp_param_list_add(nullptr), -1);
    EXPECT_EQ(fprimecsp_param_pull_u32(nullptr, 1, 2, 2, &value), -1);
    EXPECT_EQ(fprimecsp_param_pull_u32(&parameter, 1, 2, 2, nullptr), -1);
    EXPECT_EQ(fprimecsp_param_push_u32(nullptr, 1, 2, 2, 3), -1);
    fprimecsp_param_serve(nullptr);
    EXPECT_EQ(TestDoubles::paramState().listAddCalls, 0);
    EXPECT_EQ(TestDoubles::paramState().pullCalls, 0);
    EXPECT_EQ(TestDoubles::paramState().pushCalls, 0);
    EXPECT_EQ(TestDoubles::paramState().serveCalls, 0);
}

void bridgeServesNonNullPackets() {
    TestDoubles::resetParam();
    csp_packet_t packet{};
    fprimecsp_param_serve(&packet);
    EXPECT_EQ(TestDoubles::paramState().serveCalls, 1);
    EXPECT_TRUE(TestDoubles::paramState().servedPacket == &packet);
}

}  // namespace

int main(int argc, char** argv) {
    return TestSupport::runSelected(
        argc,
        argv,
        {{"registry_forwards_descriptor_and_status", registryForwardsDescriptorAndStatus},
         {"server_binds_standard_callback_once", serverBindsStandardCallbackOnce},
         {"pull_u32_forwards_request_and_reads_on_success", pullU32ForwardsRequestAndReadsOnSuccess},
         {"pull_u32_preserves_output_on_failure", pullU32PreservesOutputOnFailure},
         {"push_u32_forwards_value_and_ack_policy", pushU32ForwardsValueAndAckPolicy},
         {"invalid_protocol_version_is_rejected", invalidProtocolVersionIsRejected},
         {"bridge_guards_null_inputs", bridgeGuardsNullInputs},
         {"bridge_serves_non_null_packets", bridgeServesNonNullPackets}});
}
