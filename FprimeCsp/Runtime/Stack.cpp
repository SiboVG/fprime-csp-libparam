#include "FprimeCsp/Runtime/Stack.hpp"

namespace FprimeCsp {
namespace {

constexpr int ERROR_ALREADY_INITIALIZED = -200;
constexpr int ERROR_PROCESS_OWNER_EXISTS = -201;
constexpr int ERROR_INVALID_PROTOCOL_VERSION = -202;
constexpr int ERROR_TRANSPORT = -203;
constexpr int ERROR_NOT_INITIALIZED = -204;
constexpr int ERROR_ROUTER_TASK = -205;
constexpr int ERROR_INVALID_LOCAL_NODE = -206;
constexpr int ERROR_INVALID_NETWORK_PREFIX = -207;

std::atomic<bool> processStackClaimed{false};

}  // namespace

Stack::Stack() = default;

Stack::~Stack() {
    this->shutdown();
}

bool Stack::initialize(const StackConfig& config, Transport& transport) {
    if (this->m_initialized || this->m_shutdown || this->m_processOwner) {
        this->m_lastError = ERROR_ALREADY_INITIALIZED;
        return false;
    }
    if ((config.protocolVersion == 0U) || (config.protocolVersion > 2U)) {
        this->m_lastError = ERROR_INVALID_PROTOCOL_VERSION;
        return false;
    }

    const std::uint8_t addressBits = (config.protocolVersion == 1U) ? 5U : 14U;
    if (config.networkPrefix >= addressBits) {
        this->m_lastError = ERROR_INVALID_NETWORK_PREFIX;
        return false;
    }
    const std::uint16_t maximumNode = static_cast<std::uint16_t>((1U << addressBits) - 1U);
    const std::uint8_t subnetHostBits = static_cast<std::uint8_t>(addressBits - config.networkPrefix);
    const std::uint16_t subnetHostMask = static_cast<std::uint16_t>((1U << subnetHostBits) - 1U);
    if ((config.localNode > maximumNode) || ((config.localNode & subnetHostMask) == subnetHostMask)) {
        this->m_lastError = ERROR_INVALID_LOCAL_NODE;
        return false;
    }

    bool unclaimed = false;
    if (!processStackClaimed.compare_exchange_strong(unclaimed, true)) {
        this->m_lastError = ERROR_PROCESS_OWNER_EXISTS;
        return false;
    }
    this->m_processOwner = true;

    this->m_hostname = config.hostname;
    this->m_model = config.model;
    this->m_revision = config.revision;

    csp_conf.version = config.protocolVersion;
    csp_conf.hostname = this->m_hostname.c_str();
    csp_conf.model = this->m_model.c_str();
    csp_conf.revision = this->m_revision.c_str();
    csp_conf.conn_dfl_so = config.defaultConnectionOptions;
    csp_conf.dedup = config.deduplication;
    csp_init();

    this->m_transport = &transport;
    const int transportStatus = this->m_transport->open(config.localNode, config.networkPrefix);
    if (transportStatus != CSP_ERR_NONE) {
        this->m_lastError = (transportStatus == 0) ? ERROR_TRANSPORT : transportStatus;
        return false;
    }

    this->m_initialized = true;
    this->m_lastError = 0;
    return true;
}

bool Stack::bindCallback(const csp_callback_t callback, const std::uint8_t port) {
    if (!this->m_initialized) {
        this->m_lastError = ERROR_NOT_INITIALIZED;
        return false;
    }
    const int status = csp_bind_callback(callback, port);
    if (status != CSP_ERR_NONE) {
        this->m_lastError = status;
        return false;
    }
    this->m_lastError = 0;
    return true;
}

bool Stack::bindServiceHandler() {
    if (this->m_serviceHandlerBound) {
        return true;
    }
    if (!this->bindCallback(csp_service_handler, CSP_ANY)) {
        return false;
    }
    this->m_serviceHandlerBound = true;
    return true;
}

bool Stack::startRouter() {
    if (!this->m_initialized) {
        this->m_lastError = ERROR_NOT_INITIALIZED;
        return false;
    }
    if (this->m_routerStarted) {
        return this->m_routerRunning.load();
    }

    this->m_routerRunning.store(true);
    const Os::TaskString taskName("CspRouter");
    const Os::Task::Arguments arguments(taskName,
                                        Stack::routerEntry,
                                        this,
                                        Os::Task::TASK_PRIORITY_DEFAULT,
                                        Os::Task::TASK_DEFAULT,
                                        Os::Task::TASK_DEFAULT);
    if (this->m_routerTask.start(arguments) != Os::Task::OP_OK) {
        this->m_routerRunning.store(false);
        this->m_lastError = ERROR_ROUTER_TASK;
        return false;
    }
    this->m_routerStarted = true;
    this->m_lastError = 0;
    return true;
}

void Stack::requestStop() {
    this->m_routerRunning.store(false);
}

bool Stack::join() {
    if (!this->m_routerStarted) {
        return true;
    }
    const Os::Task::Status status = this->m_routerTask.join();
    if (status != Os::Task::OP_OK) {
        this->m_lastError = static_cast<int>(status);
        return false;
    }
    this->m_routerStarted = false;
    return true;
}

void Stack::shutdown() {
    if (this->m_shutdown) {
        return;
    }
    this->requestStop();
    (void)this->join();
    if (this->m_transport != nullptr) {
        const int status = this->m_transport->close();
        if (status != CSP_ERR_NONE) {
            this->m_lastError = status;
        }
        this->m_transport = nullptr;
    }
    this->m_initialized = false;
    this->m_shutdown = true;
}

int Stack::ping(const std::uint16_t peerNode,
                const std::uint32_t timeoutMs,
                const std::uint32_t payloadBytes) const {
    if (!this->m_initialized) {
        return -1;
    }
    return csp_ping(peerNode, timeoutMs, payloadBytes, CSP_O_CRC32);
}

bool Stack::isInitialized() const {
    return this->m_initialized;
}

bool Stack::isRouterRunning() const {
    return this->m_routerRunning.load();
}

int Stack::lastError() const {
    return this->m_lastError;
}

void Stack::routerEntry(void* context) {
    auto* stack = static_cast<Stack*>(context);
    if (stack == nullptr) {
        return;
    }
    while (stack->m_routerRunning.load()) {
        (void)csp_route_work();
    }
}

}  // namespace FprimeCsp
