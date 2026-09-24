#pragma once

#include <Os/Task.hpp>

#include <atomic>
#include <cstdint>
#include <string>
#include <type_traits>

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

/** Types of Os::Task's priority and stack size, as the OS layer defines them. */
using TaskPriority = std::remove_cv_t<decltype(Os::Task::TASK_PRIORITY_DEFAULT)>;
using TaskSize = std::remove_cv_t<decltype(Os::Task::TASK_DEFAULT)>;

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
    /**
     * Priority of the CspRouter task, on F Prime's scale (higher is more
     * important). Every CSP reply passes through the router, so on an RTOS it
     * should not be below the tasks that wait for replies. The default leaves
     * the choice to the OS layer (on Zephyr: the lowest priority).
     */
    TaskPriority routerPriority = Os::Task::TASK_PRIORITY_DEFAULT;
    /** Stack size of the CspRouter task in bytes; the default leaves it to the OS layer. */
    TaskSize routerStackSize = Os::Task::TASK_DEFAULT;
};

/** Result of an ordered router and transport shutdown attempt. */
enum class ShutdownStatus : std::uint8_t {
    COMPLETE = 0,
    ROUTER_JOIN_FAILED = 1,
    TRANSPORT_CLOSE_FAILED = 2,
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
    ShutdownStatus shutdown();

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
    TaskPriority m_routerPriority = Os::Task::TASK_PRIORITY_DEFAULT;
    TaskSize m_routerStackSize = Os::Task::TASK_DEFAULT;

    // csp_conf retains pointers to these strings for the lifetime of CSP.
    std::string m_hostname;
    std::string m_model;
    std::string m_revision;
};

}  // namespace FprimeCsp
