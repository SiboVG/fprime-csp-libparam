#pragma once

#include <Os/Task.hpp>

#include <atomic>
#include <cstdint>
#include <string>

extern "C" {
#include <csp/csp.h>
}

namespace FprimeCsp {

/** Transport boundary used by the process-wide CSP stack owner. */
class Transport {
  public:
    virtual ~Transport() = default;

    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;

  protected:
    Transport() = default;

  private:
    friend class Stack;

    virtual int open(std::uint16_t localNode, std::uint8_t networkPrefix) = 0;
    virtual int close() = 0;
};

/** Process-wide libcsp configuration. The strings remain owned by Stack. */
struct StackConfig {
    std::uint16_t localNode = 0;
    std::uint8_t networkPrefix = 8;
    std::uint8_t protocolVersion = 2;
    std::string hostname = "fprime";
    std::string model = "fprime-csp";
    std::string revision = "unknown";
    std::uint32_t defaultConnectionOptions = CSP_O_NONE;
    std::uint8_t deduplication = CSP_DEDUP_OFF;
};

/**
 * Sole owner of libcsp's process-global state and router task.
 *
 * libcsp csh/2.10 has no callback unbind operation. Consequently a Stack is
 * process-final: shutdown stops owned tasks and the transport, but the process
 * must not initialize a second Stack afterward.
 */
class Stack final {
  public:
    Stack();
    ~Stack();

    Stack(const Stack&) = delete;
    Stack& operator=(const Stack&) = delete;

    bool initialize(const StackConfig& config, Transport& transport);
    bool bindCallback(csp_callback_t callback, std::uint8_t port);
    bool bindServiceHandler();

    bool startRouter();
    void requestStop();
    bool join();
    void shutdown();

    int ping(std::uint16_t peerNode, std::uint32_t timeoutMs, std::uint32_t payloadBytes) const;

    bool isInitialized() const;
    bool isRouterRunning() const;
    int lastError() const;

  private:
    static void routerEntry(void* context);

    Transport* m_transport = nullptr;
    Os::Task m_routerTask;
    std::atomic<bool> m_routerRunning{false};
    bool m_routerStarted = false;
    bool m_initialized = false;
    bool m_shutdown = false;
    bool m_processOwner = false;
    bool m_serviceHandlerBound = false;
    int m_lastError = 0;

    // csp_conf retains pointers to these strings for the lifetime of CSP.
    std::string m_hostname;
    std::string m_model;
    std::string m_revision;
};

}  // namespace FprimeCsp
