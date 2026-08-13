#include "FprimeCsp/Libparam/Libparam.hpp"

#include "FprimeCsp/Libparam/libparam_bridge.h"
#include "TestDoubles.hpp"
#include "TestSupport.hpp"

#include <cstdint>
#include <limits>

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

param_t integerDescriptor(const param_type_e type,
                          void* const storage,
                          const int arraySize = 8) {
    static std::uint16_t node = 71;
    static csp_timestamp_t timestamp{};
    param_t parameter{};
    parameter.id = 123;
    parameter.node = &node;
    parameter.type = type;
    parameter.addr = storage;
    parameter.array_size = arraySize;
    parameter.array_step = 4;
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

void pullIntegerForwardsIndexedTypedRequests() {
    struct TestCase {
        param_type_e type;
        std::int64_t value;
    };
    const TestCase cases[] = {
        {PARAM_TYPE_UINT8, 255},
        {PARAM_TYPE_INT8, -128},
        {PARAM_TYPE_UINT16, 65535},
        {PARAM_TYPE_INT16, -32768},
        {PARAM_TYPE_UINT32, 4294967295LL},
        {PARAM_TYPE_INT32, -2147483647LL - 1},
    };
    std::uint32_t storage[8]{};

    for (const auto& testCase : cases) {
        TestDoubles::resetParam();
        TestDoubles::paramState().getIntegerValue = testCase.value;
        auto parameter = integerDescriptor(testCase.type, storage);
        FprimeCsp::Client client(2);
        std::int64_t value = 0;

        EXPECT_TRUE(client.pullInteger(parameter, 71, 900, 3, value));
        const auto& state = TestDoubles::paramState();
        EXPECT_EQ(client.lastError(), 0);
        EXPECT_EQ(value, testCase.value);
        EXPECT_EQ(state.pullCalls, 1);
        EXPECT_EQ(state.pullOffset, 3);
        EXPECT_EQ(state.pullHost, 71);
        EXPECT_EQ(state.pullTimeout, 900);
        EXPECT_EQ(state.pullVersion, 2);
        EXPECT_EQ(state.getIntegerCalls, 1);
        EXPECT_EQ(state.getIntegerType, testCase.type);
        EXPECT_EQ(state.getIntegerIndex, 3U);
    }
}

void pullIntegerPreservesOutputOnFailure() {
    TestDoubles::resetParam();
    TestDoubles::paramState().pullStatus = -23;
    TestDoubles::paramState().getIntegerValue = 99;
    std::uint16_t storage[8]{};
    auto parameter = integerDescriptor(PARAM_TYPE_UINT16, storage);
    FprimeCsp::Client client;
    std::int64_t value = 77;

    EXPECT_FALSE(client.pullInteger(parameter, 71, 900, 2, value));
    EXPECT_EQ(client.lastError(), -23);
    EXPECT_EQ(value, 77);
    EXPECT_EQ(TestDoubles::paramState().getIntegerCalls, 0);
}

void pushIntegerForwardsIndexedTypedValues() {
    struct TestCase {
        param_type_e type;
        std::int64_t value;
    };
    const TestCase cases[] = {
        {PARAM_TYPE_UINT8, 255},
        {PARAM_TYPE_INT8, -128},
        {PARAM_TYPE_UINT16, 65535},
        {PARAM_TYPE_INT16, -32768},
        {PARAM_TYPE_UINT32, 4294967295LL},
        {PARAM_TYPE_INT32, -2147483647LL - 1},
    };
    std::uint32_t storage[8]{};

    for (const auto& testCase : cases) {
        TestDoubles::resetParam();
        auto parameter = integerDescriptor(testCase.type, storage);
        FprimeCsp::Client client(2);

        EXPECT_TRUE(client.pushInteger(parameter, 71, 900, 2, testCase.value));
        const auto& state = TestDoubles::paramState();
        EXPECT_EQ(client.lastError(), 0);
        EXPECT_EQ(state.pushCalls, 1);
        EXPECT_EQ(state.pushOffset, 2);
        EXPECT_EQ(state.pushHost, 71);
        EXPECT_EQ(state.pushTimeout, 900);
        EXPECT_EQ(state.pushVersion, 2);
        EXPECT_TRUE(state.pushAckWithPull);
        EXPECT_EQ(state.pushedIntegerValue, testCase.value);
    }
}

void integerOperationsRejectUnsupportedTypesBeforeIo() {
    TestDoubles::resetParam();
    std::uint64_t storage[8]{};
    auto parameter = integerDescriptor(PARAM_TYPE_UINT64, storage);
    FprimeCsp::Client client;
    std::int64_t value = 11;

    EXPECT_FALSE(client.pullInteger(parameter, 71, 900, 0, value));
    EXPECT_FALSE(client.pushInteger(parameter, 71, 900, 0, 1));
    EXPECT_EQ(value, 11);
    EXPECT_EQ(TestDoubles::paramState().pullCalls, 0);
    EXPECT_EQ(TestDoubles::paramState().pushCalls, 0);
}

void integerOperationsRejectInvalidIndexesBeforeIo() {
    TestDoubles::resetParam();
    std::uint8_t storage[200]{};
    auto parameter = integerDescriptor(PARAM_TYPE_UINT8, storage, 200);
    FprimeCsp::Client client;
    std::int64_t value = 11;

    EXPECT_FALSE(client.pullInteger(parameter, 71, 900, 200, value));
    EXPECT_FALSE(client.pushInteger(parameter, 71, 900, 200, 1));
    EXPECT_FALSE(client.pullInteger(parameter, 71, 900, 128, value));
    EXPECT_FALSE(client.pushInteger(parameter, 71, 900, 128, 1));
    EXPECT_EQ(value, 11);
    EXPECT_EQ(TestDoubles::paramState().pullCalls, 0);
    EXPECT_EQ(TestDoubles::paramState().pushCalls, 0);
}

void integerOperationsRejectInvalidDescriptorStorageBeforeIo() {
    TestDoubles::resetParam();
    std::uint16_t storage[8]{};
    FprimeCsp::Client client;
    std::int64_t value = 11;

    auto missingNode = integerDescriptor(PARAM_TYPE_UINT16, storage);
    missingNode.node = nullptr;
    EXPECT_FALSE(client.pullInteger(missingNode, 71, 900, 0, value));

    auto missingTimestamp = integerDescriptor(PARAM_TYPE_UINT16, storage);
    missingTimestamp.timestamp = nullptr;
    EXPECT_FALSE(client.pushInteger(missingTimestamp, 71, 900, 0, 1));

    auto missingStorage = integerDescriptor(PARAM_TYPE_UINT16, nullptr);
    EXPECT_FALSE(client.pullInteger(missingStorage, 71, 900, 0, value));

    auto invalidStride = integerDescriptor(PARAM_TYPE_UINT16, storage);
    invalidStride.array_step = 1;
    EXPECT_FALSE(client.pushInteger(invalidStride, 71, 900, 0, 1));

    EXPECT_EQ(value, 11);
    EXPECT_EQ(TestDoubles::paramState().pullCalls, 0);
    EXPECT_EQ(TestDoubles::paramState().pushCalls, 0);
}

void integerOperationsRejectMisalignedRamBaseBeforeIo() {
    TestDoubles::resetParam();
    alignas(std::uint32_t) std::uint8_t storage[(sizeof(std::uint32_t) * 8) + 1]{};
    auto parameter = integerDescriptor(PARAM_TYPE_UINT32, storage + 1);
    FprimeCsp::Client client;
    std::int64_t value = 11;

    EXPECT_FALSE(client.pullInteger(parameter, 71, 900, 0, value));
    EXPECT_FALSE(client.pushInteger(parameter, 71, 900, 0, 1));
    EXPECT_EQ(value, 11);
    EXPECT_EQ(TestDoubles::paramState().pullCalls, 0);
    EXPECT_EQ(TestDoubles::paramState().pushCalls, 0);
}

void integerOperationsRejectMisalignedRamStrideBeforeIo() {
    TestDoubles::resetParam();
    std::uint16_t storage[12]{};
    auto parameter = integerDescriptor(PARAM_TYPE_UINT16, storage);
    parameter.array_step = 3;
    FprimeCsp::Client client;
    std::int64_t value = 11;

    EXPECT_FALSE(client.pullInteger(parameter, 71, 900, 1, value));
    EXPECT_FALSE(client.pushInteger(parameter, 71, 900, 1, 1));
    EXPECT_EQ(value, 11);
    EXPECT_EQ(TestDoubles::paramState().pullCalls, 0);
    EXPECT_EQ(TestDoubles::paramState().pushCalls, 0);
}

void integerOperationsAllowByteAddressedVmemDescriptors() {
    struct TestCase {
        param_type_e type;
        int stride;
    };
    const TestCase cases[] = {
        {PARAM_TYPE_UINT16, 3},
        {PARAM_TYPE_UINT32, 5},
    };
    std::uint8_t vmemToken = 0;

    for (const auto& testCase : cases) {
        TestDoubles::resetParam();
        auto parameter = integerDescriptor(testCase.type, nullptr);
        parameter.vmem = reinterpret_cast<struct vmem_s*>(&vmemToken);
        parameter.vaddr = 1;
        parameter.array_step = testCase.stride;
        FprimeCsp::Client client;
        std::int64_t value = 0;

        EXPECT_TRUE(client.pullInteger(parameter, 71, 900, 1, value));
        EXPECT_TRUE(client.pushInteger(parameter, 71, 900, 1, 1));
        EXPECT_EQ(TestDoubles::paramState().pullCalls, 1);
        EXPECT_EQ(TestDoubles::paramState().pushCalls, 1);
    }
}

void integerOperationsRequireDescriptorNodeToMatchPeer() {
    TestDoubles::resetParam();
    std::uint16_t storage[8]{};
    auto parameter = integerDescriptor(PARAM_TYPE_UINT16, storage);
    FprimeCsp::Client client;
    std::int64_t value = 11;

    EXPECT_FALSE(client.pullInteger(parameter, 72, 900, 0, value));
    EXPECT_FALSE(client.pushInteger(parameter, 72, 900, 0, 1));
    EXPECT_EQ(value, 11);
    EXPECT_EQ(TestDoubles::paramState().pullCalls, 0);
    EXPECT_EQ(TestDoubles::paramState().pushCalls, 0);
}

void integerOperationsRejectTimeoutsThatCannotReachTheCApi() {
    TestDoubles::resetParam();
    std::uint16_t storage[8]{};
    auto parameter = integerDescriptor(PARAM_TYPE_UINT16, storage);
    FprimeCsp::Client client;
    std::int64_t value = 11;
    const auto oversizedTimeout = static_cast<std::uint32_t>(std::numeric_limits<int>::max()) + 1U;

    EXPECT_FALSE(client.pullInteger(parameter, 71, oversizedTimeout, 0, value));
    EXPECT_FALSE(client.pushInteger(parameter, 71, oversizedTimeout, 0, 1));
    EXPECT_EQ(value, 11);
    EXPECT_EQ(TestDoubles::paramState().pullCalls, 0);
    EXPECT_EQ(TestDoubles::paramState().pushCalls, 0);
}

void pushIntegerRejectsOutOfRangeValuesBeforeIo() {
    struct TestCase {
        param_type_e type;
        std::int64_t below;
        std::int64_t above;
    };
    const TestCase cases[] = {
        {PARAM_TYPE_UINT8, -1, 256},
        {PARAM_TYPE_INT8, -129, 128},
        {PARAM_TYPE_UINT16, -1, 65536},
        {PARAM_TYPE_INT16, -32769, 32768},
        {PARAM_TYPE_UINT32, -1, 4294967296LL},
        {PARAM_TYPE_INT32,
         static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()) - 1,
         static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()) + 1},
    };
    std::uint32_t storage[8]{};
    FprimeCsp::Client client;

    for (const auto& testCase : cases) {
        auto parameter = integerDescriptor(testCase.type, storage);
        EXPECT_FALSE(client.pushInteger(parameter, 71, 900, 0, testCase.below));
        EXPECT_FALSE(client.pushInteger(parameter, 71, 900, 0, testCase.above));
    }
    EXPECT_EQ(TestDoubles::paramState().pushCalls, 0);
}

void invalidProtocolVersionIsRejected() {
    TestDoubles::resetParam();
    auto parameter = descriptor();
    FprimeCsp::Client client(0);
    std::uint32_t value = 44;
    std::int64_t integerValue = 55;

    EXPECT_FALSE(client.pullU32(parameter, 71, 100, value));
    EXPECT_FALSE(client.pushU32(parameter, 71, 100, 11));
    EXPECT_FALSE(client.pullInteger(parameter, 71, 100, 0, integerValue));
    EXPECT_FALSE(client.pushInteger(parameter, 71, 100, 0, 11));
    EXPECT_EQ(client.lastError(), -1);
    EXPECT_EQ(value, 44U);
    EXPECT_EQ(integerValue, 55);
    EXPECT_EQ(TestDoubles::paramState().pullCalls, 0);
    EXPECT_EQ(TestDoubles::paramState().pushCalls, 0);
}

void bridgeGuardsNullInputs() {
    TestDoubles::resetParam();
    std::uint32_t value = 0;
    std::int64_t integerValue = 0;
    auto parameter = descriptor();

    EXPECT_EQ(fprimecsp_param_list_add(nullptr), -1);
    EXPECT_EQ(fprimecsp_param_pull_u32(nullptr, 1, 2, 2, &value), -1);
    EXPECT_EQ(fprimecsp_param_pull_u32(&parameter, 1, 2, 2, nullptr), -1);
    EXPECT_EQ(fprimecsp_param_push_u32(nullptr, 1, 2, 2, 3), -1);
    EXPECT_EQ(fprimecsp_param_pull_integer(nullptr, 1, 2, 2, 0, &integerValue), -1);
    EXPECT_EQ(fprimecsp_param_pull_integer(&parameter, 1, 2, 2, 0, nullptr), -1);
    EXPECT_EQ(fprimecsp_param_push_integer(nullptr, 1, 2, 2, 0, 3), -1);
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
         {"pull_integer_forwards_indexed_typed_requests", pullIntegerForwardsIndexedTypedRequests},
         {"pull_integer_preserves_output_on_failure", pullIntegerPreservesOutputOnFailure},
         {"push_integer_forwards_indexed_typed_values", pushIntegerForwardsIndexedTypedValues},
         {"integer_operations_reject_unsupported_types_before_io", integerOperationsRejectUnsupportedTypesBeforeIo},
         {"integer_operations_reject_invalid_indexes_before_io", integerOperationsRejectInvalidIndexesBeforeIo},
         {"integer_operations_reject_invalid_descriptor_storage_before_io", integerOperationsRejectInvalidDescriptorStorageBeforeIo},
         {"integer_operations_reject_misaligned_ram_base_before_io", integerOperationsRejectMisalignedRamBaseBeforeIo},
         {"integer_operations_reject_misaligned_ram_stride_before_io", integerOperationsRejectMisalignedRamStrideBeforeIo},
         {"integer_operations_allow_byte_addressed_vmem_descriptors", integerOperationsAllowByteAddressedVmemDescriptors},
         {"integer_operations_require_descriptor_node_to_match_peer", integerOperationsRequireDescriptorNodeToMatchPeer},
         {"integer_operations_reject_timeouts_that_cannot_reach_the_c_api", integerOperationsRejectTimeoutsThatCannotReachTheCApi},
         {"push_integer_rejects_out_of_range_values_before_io", pushIntegerRejectsOutOfRangeValuesBeforeIo},
         {"invalid_protocol_version_is_rejected", invalidProtocolVersionIsRejected},
         {"bridge_guards_null_inputs", bridgeGuardsNullInputs},
         {"bridge_serves_non_null_packets", bridgeServesNonNullPackets}});
}
