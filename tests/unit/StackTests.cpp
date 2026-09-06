#include "FprimeCsp/Runtime/Stack.hpp"

#include "TestDoubles.hpp"
#include "TestSupport.hpp"

#include <Os/Task.hpp>

#include <chrono>
#include <cstdint>
#include <thread>

namespace {

class RecordingTransport final : public FprimeCsp::Transport {
  public:
    int openStatus = CSP_ERR_NONE;
    int closeStatus = CSP_ERR_NONE;
    int openCalls = 0;
    int closeCalls = 0;
    std::uint16_t node = 0;
    std::uint8_t prefix = 0;

  private:
    int open(std::uint16_t localNode, std::uint8_t networkPrefix) override {
        ++this->openCalls;
        this->node = localNode;
        this->prefix = networkPrefix;
        return this->openStatus;
    }

    int close() override {
        ++this->closeCalls;
        return this->closeStatus;
    }
};

FprimeCsp::StackConfig validConfig() {
    FprimeCsp::StackConfig config;
    config.localNode = 42;
    config.networkPrefix = 8;
    config.protocolVersion = 2;
    config.hostname = "flight-computer";
    config.model = "cubespec-obc";
    config.revision = "test-revision";
    config.defaultConnectionOptions = CSP_O_CRC32;
    config.deduplication = CSP_DEDUP_ALL;
    return config;
}

void packetCallback(csp_packet_t*) {}

void initializeConfiguresCspAndTransport() {
    TestDoubles::resetCsp();
    Os::Test::resetTaskFake();
    RecordingTransport transport;
    FprimeCsp::Stack stack;
    const auto config = validConfig();

    EXPECT_TRUE(stack.initialize(config, transport));
    EXPECT_TRUE(stack.isInitialized());
    EXPECT_EQ(stack.lastError(), CSP_ERR_NONE);
    EXPECT_EQ(TestDoubles::cspState().initCalls, 1);
    EXPECT_EQ(static_cast<int>(csp_conf.version), 2);
    EXPECT_EQ(std::string(csp_conf.hostname), config.hostname);
    EXPECT_EQ(std::string(csp_conf.model), config.model);
    EXPECT_EQ(std::string(csp_conf.revision), config.revision);
    EXPECT_EQ(csp_conf.conn_dfl_so, config.defaultConnectionOptions);
    EXPECT_EQ(static_cast<int>(csp_conf.dedup), static_cast<int>(config.deduplication));
    EXPECT_EQ(transport.openCalls, 1);
    EXPECT_EQ(transport.node, config.localNode);
    EXPECT_EQ(static_cast<int>(transport.prefix), static_cast<int>(config.networkPrefix));

    stack.shutdown();
    EXPECT_FALSE(stack.isInitialized());
    EXPECT_EQ(transport.closeCalls, 1);
    stack.shutdown();
    EXPECT_EQ(transport.closeCalls, 1);
}

void invalidConfigIsRejectedBeforeCspInit() {
    TestDoubles::resetCsp();
    RecordingTransport transport;
    FprimeCsp::Stack stack;

    auto config = validConfig();
    config.protocolVersion = 3;
    EXPECT_FALSE(stack.initialize(config, transport));
    EXPECT_TRUE(stack.lastError() != 0);

    config = validConfig();
    config.networkPrefix = 14;
    EXPECT_FALSE(stack.initialize(config, transport));
    EXPECT_TRUE(stack.lastError() != 0);

    config = validConfig();
    config.localNode = 63;  // all host bits set for /8 in the CSP v2 14-bit address space
    EXPECT_FALSE(stack.initialize(config, transport));
    EXPECT_TRUE(stack.lastError() != 0);

    EXPECT_EQ(TestDoubles::cspState().initCalls, 0);
    EXPECT_EQ(transport.openCalls, 0);
}

void transportErrorIsPreserved() {
    TestDoubles::resetCsp();
    RecordingTransport transport;
    transport.openStatus = CSP_ERR_DRIVER;
    FprimeCsp::Stack stack;

    EXPECT_FALSE(stack.initialize(validConfig(), transport));
    EXPECT_FALSE(stack.isInitialized());
    EXPECT_EQ(stack.lastError(), CSP_ERR_DRIVER);
    EXPECT_EQ(transport.openCalls, 1);
}

void callbackBindingForwardsAndPreservesErrors() {
    TestDoubles::resetCsp();
    RecordingTransport transport;
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(validConfig(), transport));

    EXPECT_TRUE(stack.bindCallback(packetCallback, 37));
    EXPECT_EQ(TestDoubles::cspState().bindCalls, 1);
    EXPECT_TRUE(TestDoubles::cspState().boundCallback == packetCallback);
    EXPECT_EQ(static_cast<int>(TestDoubles::cspState().boundPort), 37);

    TestDoubles::cspState().bindStatus = CSP_ERR_ALREADY;
    EXPECT_FALSE(stack.bindCallback(packetCallback, 38));
    EXPECT_EQ(stack.lastError(), CSP_ERR_ALREADY);
}

void serviceHandlerBindingIsIdempotent() {
    TestDoubles::resetCsp();
    RecordingTransport transport;
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(validConfig(), transport));

    EXPECT_TRUE(stack.bindServiceHandler());
    EXPECT_TRUE(stack.bindServiceHandler());
    EXPECT_EQ(TestDoubles::cspState().bindCalls, 1);
    EXPECT_TRUE(TestDoubles::cspState().boundCallback == csp_service_handler);
    EXPECT_EQ(static_cast<int>(TestDoubles::cspState().boundPort), CSP_ANY);
}

void routerRunsStopsAndJoins() {
    TestDoubles::resetCsp();
    Os::Test::resetTaskFake();
    RecordingTransport transport;
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(validConfig(), transport));

    EXPECT_TRUE(stack.startRouter());
    EXPECT_TRUE(stack.startRouter());
    for (int attempt = 0; (attempt < 1000) && (TestDoubles::cspState().routeCalls.load() == 0); ++attempt) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    EXPECT_TRUE(stack.isRouterRunning());
    EXPECT_TRUE(TestDoubles::cspState().routeCalls.load() > 0);
    EXPECT_EQ(Os::Test::taskStartCalls, static_cast<std::size_t>(1));
    EXPECT_EQ(Os::Test::lastTaskName, std::string("CspRouter"));

    stack.requestStop();
    EXPECT_TRUE(stack.join());
    EXPECT_FALSE(stack.isRouterRunning());
    EXPECT_EQ(Os::Test::taskJoinCalls, static_cast<std::size_t>(1));
}

void routerStartErrorIsReported() {
    TestDoubles::resetCsp();
    Os::Test::resetTaskFake();
    Os::Test::taskStartStatus = Os::Task::ERROR_RESOURCES;
    RecordingTransport transport;
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(validConfig(), transport));

    EXPECT_FALSE(stack.startRouter());
    EXPECT_FALSE(stack.isRouterRunning());
    EXPECT_TRUE(stack.lastError() != 0);
    EXPECT_EQ(Os::Test::taskStartCalls, static_cast<std::size_t>(1));
    EXPECT_EQ(Os::Test::taskJoinCalls, static_cast<std::size_t>(0));
}

void shutdownDefersTransportCloseUntilRouterJoinSucceeds() {
    TestDoubles::resetCsp();
    Os::Test::resetTaskFake();
    RecordingTransport transport;
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(validConfig(), transport));
    EXPECT_TRUE(stack.startRouter());

    Os::Test::taskJoinStatus = Os::Task::JOIN_ERROR;
    EXPECT_EQ(static_cast<int>(stack.shutdown()),
              static_cast<int>(FprimeCsp::ShutdownStatus::ROUTER_JOIN_FAILED));
    EXPECT_EQ(transport.closeCalls, 0);
    EXPECT_TRUE(stack.isInitialized());
    EXPECT_EQ(stack.lastError(), static_cast<int>(Os::Task::JOIN_ERROR));

    Os::Test::taskJoinStatus = Os::Task::OP_OK;
    EXPECT_EQ(static_cast<int>(stack.shutdown()),
              static_cast<int>(FprimeCsp::ShutdownStatus::COMPLETE));
    EXPECT_EQ(transport.closeCalls, 1);
    EXPECT_FALSE(stack.isInitialized());
}

void shutdownRetriesTransportCloseFailure() {
    TestDoubles::resetCsp();
    Os::Test::resetTaskFake();
    RecordingTransport transport;
    transport.closeStatus = CSP_ERR_DRIVER;
    FprimeCsp::Stack stack;
    EXPECT_TRUE(stack.initialize(validConfig(), transport));

    EXPECT_EQ(static_cast<int>(stack.shutdown()),
              static_cast<int>(FprimeCsp::ShutdownStatus::TRANSPORT_CLOSE_FAILED));
    EXPECT_EQ(transport.closeCalls, 1);
    EXPECT_TRUE(stack.isInitialized());

    transport.closeStatus = CSP_ERR_NONE;
    EXPECT_EQ(static_cast<int>(stack.shutdown()),
              static_cast<int>(FprimeCsp::ShutdownStatus::COMPLETE));
    EXPECT_EQ(transport.closeCalls, 2);
    EXPECT_FALSE(stack.isInitialized());
}

void pingUsesCrc32AndRequiresInitialization() {
    TestDoubles::resetCsp();
    RecordingTransport transport;
    FprimeCsp::Stack stack;
    EXPECT_EQ(stack.ping(71, 500, 16), -1);
    EXPECT_EQ(TestDoubles::cspState().pingCalls, 0);
    EXPECT_TRUE(stack.initialize(validConfig(), transport));
    TestDoubles::cspState().pingStatus = 12;

    EXPECT_EQ(stack.ping(71, 500, 16), 12);
    EXPECT_EQ(TestDoubles::cspState().pingCalls, 1);
    EXPECT_EQ(TestDoubles::cspState().pingNode, static_cast<std::uint16_t>(71));
    EXPECT_EQ(TestDoubles::cspState().pingTimeout, static_cast<std::uint32_t>(500));
    EXPECT_EQ(TestDoubles::cspState().pingSize, 16U);
    EXPECT_EQ(static_cast<int>(TestDoubles::cspState().pingOptions), CSP_O_CRC32);
}

}  // namespace

int main(int argc, char** argv) {
    return TestSupport::runSelected(
        argc,
        argv,
        {{"initialize_configures_csp_and_transport", initializeConfiguresCspAndTransport},
         {"invalid_config_is_rejected_before_csp_init", invalidConfigIsRejectedBeforeCspInit},
         {"transport_error_is_preserved", transportErrorIsPreserved},
         {"callback_binding_forwards_and_preserves_errors", callbackBindingForwardsAndPreservesErrors},
         {"service_handler_binding_is_idempotent", serviceHandlerBindingIsIdempotent},
         {"router_runs_stops_and_joins", routerRunsStopsAndJoins},
         {"router_start_error_is_reported", routerStartErrorIsReported},
         {"shutdown_defers_transport_close_until_router_join_succeeds",
          shutdownDefersTransportCloseUntilRouterJoinSucceeds},
         {"shutdown_retries_transport_close_failure", shutdownRetriesTransportCloseFailure},
         {"ping_uses_crc32_and_requires_initialization", pingUsesCrc32AndRequiresInitialization}});
}
